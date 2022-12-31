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

#include <array>
#include <atomic>
#include <thread>
#include <assert.h>
#include <cstddef>

#include "arena_allocator.h"
#include "core/arena_allocator.h"
#include "core/types.h"

namespace lock_free {

inline namespace LIB_VERSION {

/***
 * @brief A generic free-lock queue that can be used in different arena_allocator.
 *        Moreover, the same class provides four different implementations in order to cover 
 *        basically all applications needs. 
 * 
 * @tparam data_t        data type held by the queue
 * @tparam data_size_t   data type to be used internally for counting and sizing. 
 *                       This is required to be 32 bits or 64 bits in order to keep performances.
 * @tparam imp_type      allows selecting which kind of implementation we need:
 *                       - raw      : the queue should be used in a single thread context since there are no
 *                                    mutex or other synchronization mechanism in the implementation.
 *                       - mutex    : the queue will used the standard std::mutex to synchronise r/e access 
 *                       - spinlock : the queue will used a spinlock mutex, exactly core::mutex 
 *                                    to synchronise r/e access 
 *                       - lockfree : read write operation will be done using atomics and classic CAS loop.
 * @tparam chunk_size    number of data_t items to pre-alloc each time that is needed.
 * @tparam reserve_size  reserved size for the queue, this size will be reserved when the object is created.
 *                       When application level know the amount of memory items to be used this parameter allow
 *                       to improve significantly performances as well as to avoid fragmentation.
 * @tparam size_limit    default value is 0 that means the queue can grow until there is available memory.
 *                       A value different greater than 0 will have the effect to limit max number of items on 
 *                       the queue.
 * @tparam arena_t       lock_free::arena_allocator (default), core::arena_allocator or user defined arena allocator.
*/
template<typename data_t, typename data_size_t, core::ds_impl_t imp_type, 
         data_size_t chunk_size = 1024, data_size_t reserve_size = chunk_size, data_size_t size_limit = 0,
         typename arena_t = lock_free::arena_allocator<core::node_t<data_t,false,true,(imp_type==core::ds_impl_t::lockfree)>, data_size_t, chunk_size, reserve_size, size_limit, (chunk_size / 3), core::default_allocator<data_size_t>> >
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,uint32_t> || std::is_same_v<data_size_t,uint64_t>)
         && ( ((sizeof(data_t) % alignof(std::max_align_t)) == 0 ) || ((sizeof(std::max_align_t) % alignof(data_t)) == 0 ) )
         && (chunk_size >= 1)
class queue : core::plug_mutex<(imp_type==core::ds_impl_t::mutex)||(imp_type==core::ds_impl_t::spinlock), std::conditional_t<(imp_type==core::ds_impl_t::spinlock),core::mutex,std::mutex>>
{
public:
  using value_type      = data_t; 
  using size_type       = data_size_t;
  using reference       = data_t&;
  using const_reference = const data_t&;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;
  using node_type       = core::node_t<data_t,false,true,(imp_type==core::ds_impl_t::lockfree)>;
  using plug_mutex_type = core::plug_mutex<(imp_type==core::ds_impl_t::mutex)||(imp_type==core::ds_impl_t::spinlock), std::conditional_t<(imp_type==core::ds_impl_t::spinlock),core::mutex,std::mutex> >;
  using node_pointer    = std::conditional_t<(imp_type==core::ds_impl_t::lockfree),std::atomic<node_type*>,node_type*>;
  using arena_type      = arena_t;

public:
  
  /***/
  constexpr inline queue()
    : _head(nullptr), _tail(nullptr)
  {
    if constexpr (imp_type==core::ds_impl_t::lockfree)
    {
      _head.store( nullptr, std::memory_order_release );
      _tail.store( nullptr, std::memory_order_release );
    }
  }

  /***/
  constexpr inline ~queue() noexcept
  {
    clear();
  }

  /**
   * @brief Query how many items are present in the queue.
   * 
   * @return the number of items queued.
   */
  constexpr inline size_type       size()  const noexcept
  { return _size_imp(); }

  /**
   * @brief Query if how many items are present in the queue.
   * 
   * @return true    if there are no items, false otherwise.
   */
  constexpr inline bool            empty() const noexcept
  { return (size()==0); }

  /***/
  template<typename value_type>
  constexpr inline core::result_t  push( value_type&& data ) noexcept
  {
    if constexpr (imp_type==core::ds_impl_t::lockfree)
      return _push_imp_lockfree( std::move(data) );
    
    if constexpr (imp_type!=core::ds_impl_t::lockfree)
      return _push_imp_default( std::move(data) );

    return core::result_t::eNotImplemented;
  }

  /**
   * @brief Extract first element from the queue.
   * 
   * @param data                            output parameter updated only in case of success.
   * @return core::result_t::eEmpty         if there are no item in the queue.
   *         core::result_t::eSuccess       if @param data have been populated with element
   *                                        extracted from queue.
   *         core::result_t::eDoubleDelete  an internal double free have been detected.
   */
  constexpr inline core::result_t  pop( value_type& data ) noexcept
  {
    if constexpr (imp_type==core::ds_impl_t::lockfree)
      return _pop_imp_lockfree(data);
    
    if constexpr (imp_type!=core::ds_impl_t::lockfree)
      return _pop_imp_default(data);
    
    return core::result_t::eNotImplemented;
  }

  /**
   * Clear all items from current queue, releasing the memory.
  */
  constexpr inline void            clear() noexcept
  {
    if constexpr (imp_type==core::ds_impl_t::lockfree)
    {
      _tail.store( nullptr, std::memory_order_release );
      _head.store( nullptr, std::memory_order_release );
    }
    
    if constexpr (imp_type!=core::ds_impl_t::lockfree)
    {
      _tail = nullptr;
      _head = nullptr;
    }

    _arena.clear();
  }

  /**
   * @brief Lock access to the queue.
   * 
   * @return core::result_t::eSuccess         if lock() operation succeded, or
   *         core::result_t::eNotImplemented  if imp_type is raw or lockfree. 
   */
  constexpr inline core::result_t lock() const noexcept
  {
    if constexpr (plug_mutex_type::has_mutex==true)
    { 
      do{
      } while ( !plug_mutex_type::try_lock() );
      
      return core::result_t::eSuccess;
    }

    return core::result_t::eNotImplemented;
  }

  /**
   * @brief Unlock access to the queue.
   * 
   * @return core::result_t::eSuccess         if unlock() operation succeded, or
   *         core::result_t::eNotImplemented  if imp_type is raw or lockfree. 
   */
  constexpr inline core::result_t unlock() const noexcept
  {
    if constexpr (plug_mutex_type::has_mutex==true)
    { 
      plug_mutex_type::unlock(); 
      return core::result_t::eSuccess;
    }

    return core::result_t::eNotImplemented;
  }  

private:
  /**
   * @brief Create a node object retrieving it from preallocated nodes and
   *        if required allocate a new chunck of nodes.
   * 
   * @param data  data to push in the queue.
   * @return constexpr node_t*  return a pointer to the node with 'data' and 'next' initialized. 
   */
  constexpr inline node_type*         create_node ( value_type&& data ) noexcept
  { return _arena.allocate( std::move(data) ); }

  /**
   * @brief Make specified node available for future use.
   * 
   * @param node  pointer to node to be used from create_node()
   */
  constexpr inline core::result_t     destroy_node( node_type* node ) noexcept
  { return _arena.deallocate(node); }

  /***/
  constexpr inline size_type          _size_imp()  const noexcept
  { 
    size_type ret_value = 0;

    lock();

    ret_value = _arena.length();

    unlock();

    return ret_value;
  }

  /***/
  constexpr inline core::result_t     _push_imp_default( value_type&& data ) noexcept
  {
    core::result_t ret_value = core::result_t::eFailure;

    lock();

    node_type* new_node = create_node( std::move(data) );
    if ( new_node != nullptr )
    {
      if ( _head == nullptr ) {
        _head = new_node;
        _tail = new_node;
      }
      else {
        _tail->_next = new_node;
         _tail = new_node;
      }

      ret_value = core::result_t::eSuccess;
    }

    unlock();

    return ret_value;    
  }

  /***/
  constexpr inline core::result_t     _push_imp_lockfree( value_type&& data ) noexcept
  {
    node_type* new_node = create_node( std::move(data) );
    if ( new_node == nullptr )
      return core::result_t::eFailure;

    node_type* old_tail      = nullptr;
    node_type* old_tail_next = nullptr;
    node_type* old_head      = nullptr;
    for (;;)
    {
      old_tail = _tail.load( std::memory_order_acquire );
      old_head = _head.load( std::memory_order_acquire );

      if ( ( old_head == nullptr ) && ( old_tail != nullptr ) )
      {
        _tail.compare_exchange_weak( old_tail, nullptr, std::memory_order_seq_cst,  std::memory_order_relaxed );
        continue;
      }

      if ( old_tail == nullptr ) // means that queue is empty?
      {
        if ( _tail.compare_exchange_weak( old_tail, new_node, std::memory_order_seq_cst,  std::memory_order_relaxed ) == false )
          continue; // when this fails means that _tail have been modified by a different thread, so let's come back to the loop reading the new tail.

        if ( old_head == nullptr )
        { _head.compare_exchange_strong( old_head, new_node, std::memory_order_seq_cst, std::memory_order_relaxed ); }
      }
      else
      {
        old_tail_next = old_tail->_next.load( std::memory_order_acquire );
        if ( old_tail_next != nullptr )
          continue;   // tail._next must be nullptr or it means that another thread already updated it even old_tail hasn't been updated yet.

        if ( old_tail->_next.compare_exchange_weak( old_tail_next, new_node, std::memory_order_seq_cst,  std::memory_order_relaxed ) == false )
          continue;

        // _tail at this stage can be modified only from the thread that was able to step up to here, so we do not need to check if someone else
        // is tring to update it.
        _tail.exchange( new_node, std::memory_order_release );
      }
      
      break;
    }

    return core::result_t::eSuccess;
  }

  /***/
  constexpr inline core::result_t     _pop_imp_default( value_type& data ) noexcept
  {
    core::result_t ret_value = core::result_t::eEmpty;

    lock();

    if ( _head != nullptr )  
    {
      node_type* first_node = _head;
      
      _head = _head->_next;

      data = std::move(first_node->_data);
      ret_value = destroy_node(first_node);

      if ( _head == nullptr )
        _tail = nullptr;
    }

    unlock();

    return ret_value;    
  }

  /***/
  constexpr inline core::result_t     _pop_imp_lockfree( value_type& data ) noexcept
  {
    node_type* old_head      = nullptr; 
    node_type* old_head_next = nullptr;
    for (;;)
    {
      old_head = _head.load( std::memory_order_acquire );;
      if ( old_head == nullptr )
        return core::result_t::eEmpty;

      old_head_next = old_head->_next.load( std::memory_order_acquire );

      if ( _head.compare_exchange_weak( old_head, old_head_next, std::memory_order_acq_rel, std::memory_order_acquire ) == false )
        continue;

      break;
    }
    
    data = std::move(old_head->_data);
    old_head->_next = nullptr;

    // if old_head have been already released, this may result in 
    // a logic issue at application level. 
    // core::result_t::eDoubleFree will be returned in this case.   
    return destroy_node(old_head);
  }

private:
  arena_type     _arena;
  node_pointer   _head;
  node_pointer   _tail;
  
};

} // namespace LIB_VERSION 

}

#endif // LOCK_FREE_QUEUE_H