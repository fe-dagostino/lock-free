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

#ifndef LOCK_FREE_STACK_H
#define LOCK_FREE_STACK_H

#include <syncstream>
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
 * @brief A generic free-lock stack that can be used in different arena_allocator.
 *        Moreover, the same class provides four different implementations in order to cover 
 *        basically all applications needs. 
 * 
 * @tparam data_t        data type held by the stack
 * @tparam data_size_t   data type to be used internally for counting and sizing. 
 *                       This is required to be 32 bits or 64 bits in order to keep performances.
 * @tparam imp_type      allows selecting which kind of implementation we need:
 *                       - raw      : the stack should be used in a single thread context since there are no
 *                                    mutex or other synchronization mechanism in the implementation.
 *                       - mutex    : the stack will used the standard std::mutex to synchronise r/e access 
 *                       - spinlock : the stack will used a spinlock mutex, exactly core::mutex 
 *                                    to synchronise r/e access 
 *                       - lockfree : read write operation will be done using atomics and classic CAS loop.
 * @tparam chunk_size    number of data_t items to pre-alloc each time that is needed.
 * @tparam reserve_size  reserved size for the stack, this size will be reserved when the object is created.
 *                       When application level know the amount of memory items to be used this parameter allow
 *                       to improve significantly performances as well as to avoid fragmentation.
 * @tparam size_limit    default value is 0 that means the stack can grow until there is available memory.
 *                       A value different greater than 0 will have the effect to limit max number of items on 
 *                       the stack.
 * @tparam arena_t       lock_free::arena_allocator (default), core::arena_allocator or user defined arena allocator.
*/
template<typename data_t, typename data_size_t, core::ds_impl_t imp_type, 
         data_size_t chunk_size = 1024, data_size_t reserve_size = chunk_size, data_size_t size_limit = 0,
         typename arena_t = lock_free::arena_allocator<core::node_t<data_t,false,true,(imp_type==core::ds_impl_t::lockfree)>, data_size_t, chunk_size, reserve_size, size_limit, (chunk_size / 3), core::default_allocator<data_size_t>> >
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,uint32_t> || std::is_same_v<data_size_t,uint64_t>)
         && ( ((sizeof(data_t) % alignof(std::max_align_t)) == 0 ) || ((sizeof(std::max_align_t) % alignof(data_t)) == 0 ) )
         && (chunk_size >= 1)
class stack : core::plug_mutex<(imp_type==core::ds_impl_t::mutex)||(imp_type==core::ds_impl_t::spinlock), std::conditional_t<(imp_type==core::ds_impl_t::spinlock),core::mutex,std::mutex>>
{
public:
  using value_type      = data_t; 
  using size_type       = data_size_t;
  using reference       = data_t&;
  using const_reference = const data_t&;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;
  using node_type       = core::node_t<value_type,false,true,(imp_type==core::ds_impl_t::lockfree)>;
  using plug_mutex_type = core::plug_mutex<(imp_type==core::ds_impl_t::mutex)||(imp_type==core::ds_impl_t::spinlock), std::conditional_t<(imp_type==core::ds_impl_t::spinlock),core::mutex,std::mutex>>;
  using node_addr_type  = node_type*;
  using node_pointer    = std::conditional_t<(imp_type==core::ds_impl_t::lockfree),std::atomic<node_addr_type>,node_type*>;
  using arena_type      = arena_t;

public:
  
  /***/
  constexpr inline stack()
    : _head { nullptr }
  {
    if constexpr (imp_type==core::ds_impl_t::lockfree)
    {
      _head.store( nullptr, std::memory_order_release );
    }
  }

  /***/
  constexpr inline ~stack() noexcept
  {
    clear();
  }

  /**
   * @brief Query how many items are present in the stack.
   * 
   * @return the number of items stacked.
   */
  constexpr inline size_type       size()  const noexcept
  { return _size_imp(); }

  /**
   * @brief Query if how many items are present in the stack.
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
      return _push_imp_lockfree(data);
    
    if constexpr (imp_type!=core::ds_impl_t::lockfree)
      return _push_imp_default(data);
    
    return core::result_t::eNotImplemented;
  }

  /**
   * @brief Extract first element from the stack.
   * 
   * @param data                            output parameter updated only in case of success.
   * @return core::result_t::eEmpty         if there are no item in the stack.
   *         core::result_t::eSuccess       if @param data have been populated with element
   *                                        extracted from stack.
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
   * Clear all items from current stack, releasing the memory.
  */
  constexpr inline void            clear() noexcept
  {
    if constexpr (imp_type==core::ds_impl_t::lockfree)
    {
      _head.store( nullptr, std::memory_order_release );
    }
    
    if constexpr (imp_type!=core::ds_impl_t::lockfree)
    {
      _head = nullptr;
    }

    _arena.clear();
  }

  /**
   * @brief Lock access to the stack.
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
   * @brief Unlock access to the stack.
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
   * @param data  data to push in the stack.
   * @return constexpr node_t*  return a pointer to the node with 'data' and 'next' initialized. 
   */
  template<typename value_type>
  constexpr inline node_type*         create_node ( value_type& data ) noexcept
  { return _arena.allocate(data); }

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

  template<typename value_type>
  constexpr inline core::result_t     _push_imp_default( value_type&& data ) noexcept
  {
    core::result_t ret_value = core::result_t::eFailure;

    lock();

    node_type* new_node = create_node(data);
    if ( new_node != nullptr )
    {
      new_node->_next = _head;
      _head = new_node;

      ret_value = core::result_t::eSuccess;
    }

    unlock();

    return ret_value;    
  }

  template<typename value_type>
  constexpr inline core::result_t     _push_imp_lockfree( value_type&& data ) noexcept
  {
    node_type* new_node = create_node(data);
    if ( new_node == nullptr )
      return core::result_t::eFailure;

    for (;;)
    {
      std::atomic_thread_fence( std::memory_order_acquire );
      node_addr_type old_head = _head.load( std::memory_order_relaxed );
      if ( old_head != nullptr )
        new_node->_next.store( old_head, std::memory_order_release );

      node_addr_type new_head( new_node );

      if ( _head.compare_exchange_weak( old_head, new_head ) == false )
        continue; // when this fails means that _head have been modified by a different thread, so let's come back to the loop reading the new head.

      break;
    }

    std::atomic_thread_fence( std::memory_order_release );

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

      data = first_node->_data;
      ret_value = destroy_node(first_node);
    }

    unlock();

    return ret_value;    
  }

  /***/
  constexpr inline core::result_t     _pop_imp_lockfree( value_type& data ) noexcept
  {
    node_addr_type old_head = nullptr;
    node_addr_type new_head = nullptr;
    for (;;)
    {
      std::atomic_thread_fence( std::memory_order_acquire );
      old_head = _head.load( std::memory_order_relaxed );
      if ( old_head == nullptr )
        return core::result_t::eEmpty;

      std::atomic_thread_fence( std::memory_order_acquire );
      new_head = old_head->_next.load(std::memory_order_relaxed);

      if ( _head.compare_exchange_weak( old_head, new_head ) == false )
        continue;

      break;
    };

    std::atomic_thread_fence( std::memory_order_release );

    data = old_head->_data;

    // if old_head have been already released, this may result in 
    // a logic issue at application level. 
    // core::result_t::eDoubleFree will be returned in this case.   
    return destroy_node(old_head);    
  }

private:
  arena_type     _arena;
  node_pointer   _head;
};

} // namespace LIB_VERSION 

}

#endif // LOCK_FREE_STACK_H