#ifndef LOCK_FREE_MULTI_QUEUE_H
#define LOCK_FREE_MULTI_QUEUE_H

#include <array>
#include <atomic>

namespace lock_free {

inline namespace LIB_VERSION {

/***
 * data_t        data type held by the multi queue
 * data_size_t   data type to be used internally for counting and sizing. 
 *               This is required to be 32 bits or 64 bits in order to keep performances.
 * threads       allow to specify number of threads that should work with the data structure,
 *               for each thread will be allocated a dedicated queue.
 *               Best approach for performances for example on a 16 core machine should be to keep
 *               8 x producer and 8 x consumer, but also multi producer single consumer are allowed.
 * chunk_size    number of data_t items to pre-alloc each time that is needed.
 * reserve_size  reserved size for each queue, this size will be reserved when the object is created.
 *               When application level know the amount of memory items to be used this parameter allow
 *               to improve significantly performances as well as to avoid fragmentation.
 * max_size      default value is 0 that means the queue can grow until there is available memory.
 *               A value different greater than 0 will have the effect to limit max number of items on 
 *               the single queue.
*/
template<typename data_t, typename data_size_t, data_size_t threads, 
         data_size_t chunk_size = 64, data_size_t reserve_size = chunk_size, data_size_t max_size = 0 >
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,u_int32_t> || std::is_same_v<data_size_t,u_int64_t>)
class multi_queue
{
public:
  using value_type      = data_t; 
  using size_type       = data_size_t;
  using thread_id       = data_size_t;
  using reference       = data_t&;
  using const_reference = const data_t&;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;
 
public:
  
  struct node_t
  {
    /***/
    constexpr inline node_t() noexcept
      : _next{nullptr}
    {}
    /***/
    constexpr inline node_t( value_type&& value ) noexcept
      : _next{nullptr}, _data(value)
    {}
    /***/
    constexpr inline ~node_t() noexcept
    { }

    /***/
    constexpr inline void * operator new(size_t size) noexcept
    { return aligned_alloc( alignof(node_t), size ); }
    /***/
    constexpr inline void * operator new(size_t size, [[maybe_unused]] const std::nothrow_t& nothrow_value) noexcept
    { return aligned_alloc( alignof(node_t), size ); }
    /***/
    constexpr inline void* operator new ( [[maybe_unused]] std::size_t size, void* ptr) noexcept
    { return ptr; }
    /***/
    constexpr inline void operator delete(void * ptr) noexcept
    { free(ptr); }

    std::atomic<node_t*>   _next;
    value_type             _data;
  };


  struct queue_t
  {
    constexpr inline queue_t()
      : _items{0}, _head{nullptr}, _tail{nullptr}, _buff{nullptr}
    {
      reserve(reserve_size);
    }

    constexpr inline ~queue_t() noexcept
    {
      release();
    }

    /***/
    constexpr inline size_type size() const noexcept
    { return _items.load(std::memory_order_acquire); }

    /***/
    template<typename value_type>
    constexpr inline bool push( value_type&& data ) noexcept
    {
      //node_t* new_node = new(std::nothrow) node_t(data);
      node_t* new_node = create_node(data);
      if ( new_node == nullptr )
        return false;

      node_t* old_tail = _tail.exchange( new_node, std::memory_order_seq_cst );

      if (old_tail == nullptr)
      {
        // old_tail == nullptr means that queue is empty, so in this 
        // case we also exchange current head with current node since
        // current node is the only one in the queue then both head and tail.
        _head.exchange( new_node, std::memory_order_seq_cst );
      }
      else
      {
        old_tail->_next.exchange( new_node, std::memory_order_seq_cst );
      }

      ++_items;

      return true;
    }

    /***/
    constexpr inline bool      pop( value_type& data ) noexcept
    {
      node_t* cur_head = _head.load( std::memory_order_acquire );

      // Specified queue is empty.
      if ( cur_head == nullptr )
        return false;

      node_t* old_head = cur_head;
      if ( _head.compare_exchange_strong( cur_head, cur_head->_next.load(std::memory_order_acquire) ) == false )
        return false;

      data = old_head->_data;
      destroy_node(old_head);
      
      --_items;

      return true;
    }

    /***/
    template<typename value_type>
    constexpr inline node_t* create_node ( value_type& data ) noexcept
    {  
      node_t* new_node = nullptr;
      while ( !(new_node = _buff.load( std::memory_order_acquire )) )
      {
        reserve(chunk_size);
      }

      if ( _buff.compare_exchange_strong( new_node, new_node->_next, std::memory_order_release ) == false )
        return nullptr;

      new_node->_data = data;
      new_node->_next = nullptr;

      return new_node; 
    }

    /***/
    constexpr inline void    destroy_node( node_t* node ) noexcept
    {  
      node_t* buff_node = _buff.exchange( node, std::memory_order_seq_cst );
      node->_next = buff_node;      
    }

    /***/
    constexpr inline bool    reserve( size_type items ) noexcept
    {
      if (items == 0) 
        return false;
      
      if (max_size>0)
      { 
        if ( size() >= max_size)
          return false;
      }

      node_t* new_buff  = new(std::nothrow) node_t();
      node_t* cur_next  = new_buff;
      while (items--)
      {
        cur_next->_next = new(std::nothrow) node_t();
        if (cur_next->_next == nullptr)
        {
          return false;
        }

        cur_next = cur_next->_next;
      }

      node_t* buff_node = _buff.exchange( new_buff, std::memory_order_seq_cst );
      if ( buff_node != nullptr )
        delete buff_node;

      return true;
    }

    /***/
    constexpr inline void    release() noexcept
    {
      _tail.store( nullptr, std::memory_order_release );

      node_t* head_cursor = nullptr; 
      while ( (head_cursor = _head.load(std::memory_order_acquire)) != nullptr )
      {
        _head.exchange( head_cursor->_next );
        delete head_cursor;
      }

      node_t* buff_cursor = nullptr; 
      while ( (buff_cursor = _buff.load(std::memory_order_acquire)) != nullptr )
      {
        _buff.exchange( buff_cursor->_next );
        delete buff_cursor;
      }

      _items = 0;
    }
 
    std::atomic<size_type>    _items;
    std::atomic<node_t*>      _head;
    std::atomic<node_t*>      _tail;
    std::atomic<node_t*>      _buff;
  };

  static constexpr const size_type data_type_size = sizeof(value_type);
  static constexpr const size_type node_type_size = sizeof(node_t);

public:
  /***/
  constexpr multi_queue()
  {

  }

  /***/
  constexpr inline size_type size() const noexcept
  { 
    size_type _size = 0;
    for( auto & queue : m_array ) 
      _size += queue.size(); 
    return _size;
  }

  /***/
  constexpr inline size_type size( const thread_id& id ) const noexcept
  { 
    return m_array[id].size(); 
  }

 
  /***/
  template<typename value_type>
  constexpr inline bool      push( const thread_id& id, value_type&& data ) noexcept
  {
    return m_array[id].push( std::forward<value_type>(data) );
  }
 
  /***/
  constexpr inline bool      pop( const thread_id& id, value_type& data ) noexcept
  {
    return m_array[id].pop( data );
  }

protected:

private:
  std::array<queue_t, threads>  m_array;
};

} // namespace LIB_VERSION 

}

#endif //LOCK_FREE_MULTI_QUEUE_H