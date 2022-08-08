/**************************************************************************************************
 * 
 * Copyright 2022 https://github.com/fe-dagostino
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this 
 * software and associated documentation files (the "Software"), to deal in the Software 
 * without restriction, including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
 * permit persons to whom the Software is furnished to do so, subject to the following 
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies 
 * or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 *************************************************************************************************/

#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE_H

#include <syncstream>
#include <array>
#include <atomic>
#include <assert.h>
#include <cstddef>

#include "arena_allocator.h"
#include "core/types.h"

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
template<typename data_t, typename data_size_t,  
         data_size_t chunk_size = 1024, data_size_t reserve_size = chunk_size, data_size_t max_size = 0,
         typename arena_t = arena_allocator<core::node_t<data_t,false,true,true>, data_size_t, chunk_size, reserve_size, max_size, chunk_size / 3, core::virtual_allocator<data_size_t>> >
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,u_int32_t> || std::is_same_v<data_size_t,u_int64_t>)
         && ( ((sizeof(data_t) % alignof(std::max_align_t)) == 0 ) || ((sizeof(std::max_align_t) % alignof(data_t)) == 0 ) )
         && (chunk_size >= 1)
class queue
{
public:
  using value_type      = data_t; 
  using size_type       = data_size_t;
  using queue_id        = data_size_t;
  using reference       = data_t&;
  using const_reference = const data_t&;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;
  using node_type       = core::node_t<data_t,false,true,true>;
  using arena_type      = arena_t;

public:
  
  /***/
  constexpr inline queue()
    : _items{0}, _head{nullptr}, _tail{nullptr} 
  {
    _items.store( 0, std::memory_order_release );
  }

  /***/
  constexpr inline ~queue() noexcept
  {
    clear();
  }

  /***/
  constexpr inline size_type size()  const noexcept
  { return _items.load(std::memory_order_consume); }

  /***/
  constexpr inline bool      empty() const noexcept
  { return (size()==0); }

  /***/
  template<typename value_type>
  constexpr inline bool      push( value_type&& data ) noexcept
  {
    node_type* new_node = create_node(data);
    if ( new_node == nullptr )
      return false;

    for (;;)
    {
      node_type* old_tail = _tail.load( std::memory_order_acquire );

      if ( old_tail == nullptr )
      {
        if ( _tail.compare_exchange_weak( old_tail, new_node, std::memory_order_release, std::memory_order_acquire ) == false )
          continue;

        node_type* old_head = _head.load( std::memory_order_acquire );
        if ( old_head == nullptr )
        { _head.compare_exchange_weak( old_head, new_node, std::memory_order_release, std::memory_order_relaxed ); }

        break;
      }
      else
      {
        node_type* old_next = old_tail->_next.load( std::memory_order_acquire );

        if ( old_next == nullptr )     
        {
          if ( old_tail->_next.compare_exchange_weak( old_next, new_node, std::memory_order_release, std::memory_order_acquire ) == false )
            continue;

          _tail.exchange( new_node, std::memory_order_release );
          
          node_type* old_head = _head.load( std::memory_order_acquire );
          if ( old_head == nullptr )
          { _head.compare_exchange_weak( old_head, new_node, std::memory_order_release, std::memory_order_relaxed ); }

          break;
        }
      }
    }

    size_type _cur_item = _items.load( std::memory_order_acquire );
    do {
    } while ( !_items.compare_exchange_weak( _cur_item, _cur_item+1, std::memory_order_acq_rel, std::memory_order_acquire ) ); 

    return true;
  }

  /***/
  constexpr inline bool      pop( value_type& data ) noexcept
  {
    node_type* cur_head  = _head.load( std::memory_order_acquire );
    if ( cur_head == nullptr )
    { return false; }

    do {
    } while ( cur_head && !_head.compare_exchange_weak( cur_head, cur_head->_next.load(std::memory_order_acquire), 
                                                        std::memory_order_acq_rel, std::memory_order_acquire ) );
    if ( cur_head == nullptr )
    { return false; }

    node_type* cur_tail = _tail.load( std::memory_order_acquire );
    
    if ( cur_head == cur_tail )
    { _tail.compare_exchange_weak( cur_tail, nullptr, std::memory_order_acq_rel, std::memory_order_relaxed ); }

    data = cur_head->_data;
    cur_head->_next = nullptr;

    destroy_node(cur_head);

    size_type _cur_item = _items.load( std::memory_order_acquire );
    do {
    } while ( !_items.compare_exchange_weak( _cur_item, _cur_item-1, std::memory_order_acq_rel, std::memory_order_acquire ) ); 

    return true;
  }

  /**
   * Clear all items from current queue, releasing the memory.
  */
  constexpr inline void    clear() noexcept
  {
    _tail.store( nullptr, std::memory_order_release );
    _head.store( nullptr, std::memory_order_release );
    _items = 0;

    _arena.clear();
  }

private:
  /**
   * @brief Create a node object retrieving it from preallocated nodes and
   *        if required allocate a new chunck of nodes.
   * 
   * @param data  data to push in the queue.
   * @return constexpr node_t*  return a pointer to the node with 'data' and 'next' initialized. 
   */
  template<typename value_type>
  constexpr inline node_type* create_node ( value_type& data ) noexcept
  { 
    return _arena.allocate(data);
  }

  /**
   * @brief Make specified node available for future use.
   * 
   * @param node  pointer to node to be used from create_node()
   */
  constexpr inline void       destroy_node( node_type* node ) noexcept
  { 
    _arena.deallocate(node);
  }

private:
  arena_type                _arena;
  std::atomic<size_type>    _items;
  std::atomic<node_type*>   _head;
  std::atomic<node_type*>   _tail;
 
  static constexpr const size_type data_type_size = sizeof(value_type);
  static constexpr const size_type node_type_size = sizeof(node_type);
  
};

} // namespace LIB_VERSION 

}

#endif //LOCK_FREE_QUEUE_H