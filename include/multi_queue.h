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

#ifndef LOCK_FREE_MULTI_QUEUE_H
#define LOCK_FREE_MULTI_QUEUE_H

#include <array>
#include <atomic>
#include <assert.h>
#include <cstddef>

#include "config.h"
#include "queue.h"
#include "core/arena_allocator.h"
#include "core/thread_map.h"

namespace lock_free {

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
         data_size_t chunk_size = 1024, data_size_t reserve_size = chunk_size, data_size_t max_size = 0,
         typename arena_t = lock_free::arena_allocator<core::node_t<data_t,false,true,true>, data_size_t, chunk_size, reserve_size, max_size, chunk_size / 3, core::default_allocator<data_size_t>> >
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,uint32_t> || std::is_same_v<data_size_t,uint64_t>)
         && ( ((sizeof(data_t) % alignof(std::max_align_t)) == 0 ) || ((sizeof(std::max_align_t) % alignof(data_t)) == 0 ) )
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
  using queue_type      = lock_free::queue<value_type,size_type, core::ds_impl_t::lockfree, chunk_size,reserve_size,max_size,arena_t>; 

public:
  /**
   * @brief Default Constructor.
   */
  constexpr inline multi_queue()
    : m_ndx_pop{0}
  { }

  /**
   * Clear the structure releasing all allocated memory.
   */
  constexpr inline void            clear() noexcept(true)
  { 
    for( auto & queue : m_array ) 
      queue.clear(); 
  }

  /**
   * @brief Return the number of items in the multi_queue instance.
   * 
   * @return constexpr size_type total items accross all iternal queues.
   */
  constexpr inline size_type       size() const noexcept(true)
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
  constexpr inline size_type       size( const queue_id& id ) const noexcept(true)
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
  constexpr inline core::result_t  push( const queue_id& id, value_type&& data ) noexcept(true)
  {
    assert( (id >=0) && (id < queues) );
    return m_array[id].push( std::forward<value_type>(data) );
  }

  /**/
  constexpr inline queue_id get_id() const noexcept(true)
  {
    return m_th_map.add()%queues;
  }

  /**
   * @brief Push data in a queue. Destination queue depends on thread::id.
   * 
   * @tparam value_type 
   * @param data   data to push over the queue.
   * @return true  if operation will be successfully completed.
   * @return false if queue failed to allocate memory or the queue reaches the max_size   
   */
  template<typename value_type>
  constexpr inline core::result_t  push( value_type&& data ) noexcept(true)
  {
    const queue_id id = m_th_map.add()%queues;
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
  constexpr inline core::result_t  pop( const queue_id& id, value_type& data ) noexcept(true)
  {
    assert( (id >=0) && (id < queues) );
    return m_array[id].pop( data );
  }

  /**
   * @brief Pop item from one of the queues.
   * 
   * @param data   output parameter updated only in case return value is true. 
   * @return true  in this case @param data is updated with extracted value from the selected queue queue.
   * @return false selected queue is empty, but this doesn't means that all queues are empty.
   */
  constexpr inline core::result_t  pop( value_type& data ) noexcept(true)
  { 
    size_type qid = m_ndx_pop.load( std::memory_order_acquire );
    m_ndx_pop.compare_exchange_strong( qid, ((qid+1)<queues)?(qid+1):0 );
    return m_array[qid].pop( data );
  }

protected:

private:
  mutable core::thread_map<size_type,0>  m_th_map; 
  std::array<queue_type, queues>         m_array;
  std::atomic<size_type>                 m_ndx_pop;
};

}

#endif //LOCK_FREE_MULTI_QUEUE_H