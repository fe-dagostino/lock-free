#ifndef LOCK_FREE_MULTI_QUEUE_H
#define LOCK_FREE_MULTI_QUEUE_H

#include <array>
#include <atomic>
#include <assert.h>

namespace lock_free {

inline namespace LIB_VERSION {

/***
 * @brief A generic free-lock multi-queue that can be used in different scenarios.
 *        Even there are some design decisiton that have a minimum impact on performances, this
 *        multi-queue is performing at its maximum in all uses cases where items are known 
 *        and implementation can reuse existing preallocated buffers instead to create new chunks.
 * 
 * data_t        data type held by the multi queue
 * data_size_t   data type to be used internally for counting and sizing. 
 *               This is required to be 32 bits or 64 bits in order to keep performances.
 * queues        allow to specify number of queues provided by the multi-queue data structure,
 *               for each queue will be possible to dedicate a thread or to share it between multiple threads.
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
template<typename data_t, typename data_size_t, data_size_t queues, 
         data_size_t chunk_size = 16, data_size_t reserve_size = chunk_size, data_size_t max_size = 0 >
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,u_int32_t> || std::is_same_v<data_size_t,u_int64_t>)
         && (queues >= 1) && (chunk_size >= 1)
class multi_queue
{
public:
  using value_type      = data_t; 
  using size_type       = data_size_t;
  using queue_id        = data_size_t;
  using reference       = data_t&;
  using const_reference = const data_t&;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;
 
private:
  /**
   * @brief A single node for the queue with pointer to next item.
   *        As for design decision each node_t is indipendent and 
   *        we pay the cost of this decision in memory allocation time.
   *        On the other side this keep the code readable and estensible.
   */
  struct node_t
  {
    /**
     * @brief Default constructor.
     */
    constexpr inline node_t() noexcept
      : _next{nullptr}
    { }
    /**
     * @brief Constructor taht allow to specify value for the node.
     */
    constexpr inline node_t( value_type&& value ) noexcept
      : _next{nullptr}, _data(value)
    { }
    /**
     * @brief Destructor.
     *        Note: deallocation is performed at queue_t level in order to avoid
     *              stack overflow when deallocating millions of items.
     */
    constexpr inline ~node_t() noexcept
    { }

    /**
     * @brief Overloading operator new.
     */
    constexpr inline void * operator new(size_t size) noexcept
    { return aligned_alloc( alignof(node_t), size ); }
    /**
     * @brief Overloading operator new.
     */
    constexpr inline void * operator new(size_t size, [[maybe_unused]] const std::nothrow_t& nothrow_value) noexcept
    { return aligned_alloc( alignof(node_t), size ); }
    /**
     * @brief Overloading operator new.
     */
    constexpr inline void* operator new ( [[maybe_unused]] std::size_t size, void* ptr) noexcept
    { return ptr; }
    /**
     * @brief Overloading operator delete.
     */
    constexpr inline void operator delete(void * ptr) noexcept
    { free(ptr); }

    std::atomic<node_t*>   _next;
    value_type             _data;
  };


  /**
   * @brief Internal data structure for a queue.
   *        Each single queue is lock-free and can be shared
   *        between multiple threads.
   *        This is a design assumption that impact on runtime
   *        speed but on the other side let open to different use
   *        cases. 
   */
  struct queue_t
  {
    constexpr inline queue_t()
      : _items{0}, _head{nullptr}, _tail{nullptr}, _buff{nullptr}
    {
      reserve(reserve_size);
    }

    constexpr inline ~queue_t() noexcept
    {
      clear();
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

    /**
     * @brief Create a node object retrieving it from preallocated nodes and
     *        if required allocate a new chunck of nodes.
     * 
     * @param data  data to push in the queue.
     * @return constexpr node_t*  return a pointer to the node with 'data' and 'next' initialized. 
     */
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

      // Call operator new using pre-allocated memory.
      new_node = new(new_node) node_t(data);

      return new_node; 
    }

    /**
     * @brief Make specified node available for future use.
     * 
     * @param node  pointer to node to be used from create_node()
     */
    constexpr inline void    destroy_node( node_t* node ) noexcept
    {  
      node_t* buff_node = _buff.exchange( node, std::memory_order_seq_cst );
      node->_next = buff_node;      
    }

    /**
     * @brief Reserve a specified number of items on the queue
     * 
     * @param items     number of items to be reserved.
     * @return true     in case of successfull
     * @return false    in case of out-of-memory or if size() reach max_size
     */
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

    /**
     * Clear all items from current queue, releasing the memory.
    */
    constexpr inline void    clear() noexcept
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
  /**
   * @brief Default Constructor.
   */
  constexpr inline multi_queue()
  { }

  /**
   * Clear the structure releasing all allocated memory.
  */
  constexpr inline void      clear() noexcept
  { 
    for( auto & queue : m_array ) 
      queue.clear(); 
  }

  /**
   * @brief Return the number of items in the multi_queue instance.
   * 
   * @return constexpr size_type total items accross all iternal queues.
   */
  constexpr inline size_type size() const noexcept
  { 
    size_type _size = 0;
    for( auto & queue : m_array ) 
      _size += queue.size(); 
    return _size;
  }

  /**
   * @brief Return items count for the specified thread-id that also match with the queue id.
   * 
   * @param id  specify the queue-id.
   * @return constexpr size_type   number of items for the specified queue.
   */
  constexpr inline size_type size( const queue_id& id ) const noexcept
  { 
    assert( (id >=0) && (id < queues) );
    return m_array[id].size(); 
  }

  /**
   * @brief Push data in the specified queue.
   * 
   * @tparam value_type 
   * @param id     queue-id to be used.
   * @param data   data to push over the queue.
   * @return true  if operation will be successfully completed.
   * @return false if queue failed to allocate memory or the queue reaches the max_size
   */
  template<typename value_type>
  constexpr inline bool      push( const queue_id& id, value_type&& data ) noexcept
  {
    assert( (id >=0) && (id < queues) );
    return m_array[id].push( std::forward<value_type>(data) );
  }
 
  /**
   * @brief Pop item from specified queue-id.
   * 
   * @param id      queue-id from where to pop item.
   * @param data    output parameter updated only in case return value is true.
   * @return true   in this case @param data is updated with extracted value from the queue.
   * @return false  queue is empty.
   */
  constexpr inline bool      pop( const queue_id& id, value_type& data ) noexcept
  {
    assert( (id >=0) && (id < queues) );
    return m_array[id].pop( data );
  }

  /**
   * @brief Pop item from one of the queues.
   * 
   * @param data   output parameter updated only in case return value is true. 
   * @return true  in this case @param data is updated with extracted value from the selected queue queue.
   * @return false all queues are empty.
   */
  constexpr inline bool      pop( value_type& data ) noexcept
  { 
    size_type qid = 0;
    size_type max = 0;
    for( size_type ndx = 0; ndx < queues; ++ndx ) 
    {
      size_type size = m_array[ndx].size(); 
      if (size > max)
      {
        max = size;
        qid = ndx;
      }
    }

    return m_array[qid].pop( data );
  }

protected:

private:
  std::array<queue_t, queues>  m_array;
};

} // namespace LIB_VERSION 

}

#endif //LOCK_FREE_MULTI_QUEUE_H