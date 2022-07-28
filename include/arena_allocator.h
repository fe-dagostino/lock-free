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

#ifndef LOCK_FREE_ARENA_ALLOCATOR_H
#define LOCK_FREE_ARENA_ALLOCATOR_H

#include <mutex>
#include <assert.h>

#include "memory_address.h"

namespace lock_free {

inline namespace LIB_VERSION {


/***
 * An arena_allocator implementation that keep both alloc() and dealloc() to a complexity of O(1).
 * 
 * This arena_allocator is useful when :
 *  - in a program there is an extensive use of "new" and "delete" for a well-defined "data type";
 *  - we want to avoid memory fragmentation, since the program is supposed to run for long time without interruptions;
 *  - performances in our program are important and we want to avoid waste of CPU cycles.
 * 
*/
template< typename data_t, typename data_size_t, data_size_t items = 1024> 
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,u_int32_t> || std::is_same_v<data_size_t,u_int64_t>)
         && ( ((sizeof(data_t) % alignof(std::max_align_t)) == 0 ) || ((alignof(std::max_align_t) % sizeof(data_t)) == 0 ) )
         && ( items > 0 ) && ((sizeof(void*)==4) || (sizeof(void*)==8))
class arena_allocator
{
public:
  using value_type      = data_t; 
  using size_type       = data_size_t;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;

  static constexpr const size_type value_type_size = sizeof(value_type);
  
private:
  /***/
  struct memory_slot {
    /***/
    constexpr inline memory_slot() noexcept
      :_ptr_next(nullptr)
    {}
    /***/
    constexpr inline memory_slot( memory_slot* next ) noexcept
      :_ptr_next(next)
    {}

    /**
     * @brief Return pointer to user data.
     */
    constexpr inline memory_slot* next() noexcept
    { return _ptr_next; }

    /**
     * @brief Return pointer to user data.
     */
    constexpr inline pointer      prt() noexcept
    { return &_user_data; }

    /***/
    constexpr inline bool         in_use() const noexcept
    { return _ptr_next.test_flag(memory_address<memory_slot,size_type>::address_flags::DESTROY); }

    /***/
    constexpr inline bool         is_free() const noexcept
    { return !_ptr_next.test_flag(memory_address<memory_slot,size_type>::address_flags::DESTROY); }

    /***/
    constexpr inline void         set_free( memory_slot* next_free ) noexcept
    { _ptr_next.set_address( next_free, 0 ); }

    /***/
    constexpr inline void         set_in_use() noexcept
    { _ptr_next.set_address( nullptr, (size_type)memory_address<memory_slot,size_type>::address_flags::DESTROY); }
    
    /***/
    constexpr static inline memory_slot* slot_from_user_data( pointer ptr ) noexcept
    { return std::bit_cast<memory_slot*>(std::bit_cast<char*>(ptr)-memory_address<memory_slot,size_type>::memory_address_size); }

    memory_address<memory_slot,size_type>  _ptr_next;
    value_type                             _user_data;
  };

  using slot_pointer     = memory_slot*;
  
  static constexpr const size_type memory_slot_size = sizeof(memory_slot);
  static constexpr const size_type memory_required  = memory_slot_size*items;

public:
  /***/
  constexpr arena_allocator() noexcept
    : _first_slot( nullptr ), _last_slot( nullptr ), _next_free(nullptr), _used_slots(0)
  {
    _first_slot = static_cast<slot_pointer>(std::aligned_alloc( alignof(std::max_align_t), memory_required ));
    if ( _first_slot == nullptr )
    {
      return;
    }

    _next_free  = _first_slot; // next free item initialized with first item._
    _last_slot  = _first_slot+(items-1);

    // Iterate on overall slots in order to initialize the slots chain.
    slot_pointer mem_curs = _first_slot;
    size_type    slots_nb = items; 
    while ( slots_nb-- )
    {
      mem_curs = new(mem_curs) memory_slot((slots_nb>0)?(mem_curs+1):nullptr);
      mem_curs++;
    }
  }

  /***/
  constexpr ~arena_allocator() noexcept
  {
    // Iterate on overall slots in order to invoke ~value_type() 
    // for all object currently in use.
    slot_pointer mem_curs = _first_slot;
    size_type    slots_nb = 0; 

    while ( slots_nb++ < items )
    {
      if ( mem_curs->in_use() == true )
        mem_curs->prt()->~value_type();

      mem_curs++;
    }

    // Release memory allocated in the in the constructor.
    std::free( _first_slot );
  }

  /**
   * @brief Retrieve length in bytes for one single data_t.
   * 
   * @return sizeof(data_t) 
   */
  constexpr inline size_type  type_size() const noexcept
  { return value_type_size; }

  /**
   * @brief Retrieve length in terms of items currently in the buffer.
   * 
   * @return current items in the data buffer. 
   */
  constexpr inline size_type  length() const noexcept
  { 
    _mtx_next.lock();
    size_type ret_val = _used_slots;
    _mtx_next.unlock();
    return ret_val; 
  }

  /**
   * @brief Retrieve length in terms of items currently in the buffer.
   * 
   * @return current items in the data buffer. 
   */
  constexpr inline size_type  unsafe_length() const noexcept
  { return _used_slots; }

  /**
   * @brief Return max lenght for this arena_allocator.
   * 
   * @return max number of items accordingly with @tparam items. 
   */
  constexpr inline size_type  max_length() const noexcept
  { return items; }

  /**
   * @brief  Retrieve the size in bytes currently allocated for user data.
   * 
   * @return current size for the allocated buffer. 
   */
  constexpr inline size_type  capacity() const noexcept
  { return memory_required; }

  /**
   * @brief Return the largest supported allocation size.
   */
  constexpr inline size_type  max_size() const noexcept
  { return std::numeric_limits<size_type>::max() / memory_slot_size; }
  
  /**
   * @brief Allocate memory for value_type() object and invoke
   *        constructur accordingly with parameters.
   * 
   *        Note: this function is thread safe, if you are in single
   *              thread context evaluate to use unsafe_allocate() instead.
   * 
   * @param args    list of arguments can be also empty, in such 
   *                case the default constructor will be invoked.
   *                When specified one or more arguments they should 
   *                match with a constructor for value_type() or 
   *                an error will be generated ar compile-time.
   * @return value_type* to constructed object.
   */
  template< typename... Args > 
  constexpr inline pointer    allocate( Args&&... args ) noexcept
  { 
    _mtx_next.lock();

    slot_pointer pCurrSlot = _next_free;
    if ( pCurrSlot == nullptr )
    {
      _mtx_next.unlock();
      return nullptr;
    }

    _next_free = pCurrSlot->next();
    pCurrSlot->set_in_use();
    ++_used_slots;
    
    _mtx_next.unlock();

    return new(pCurrSlot->prt()) value_type( std::forward<Args>(args)... );
  }

  /**
   * @brief Deallocate memory for @param userdata pointer and invoke ~value_type().
   * 
   *        Note: this function is thread safe, if you are in single
   *              thread context evaluate to use unsafe_allocate() instead.
   *  
   * @param userdata  pointer to user data previously allocated with allocate().
   */
  constexpr inline void       deallocate( pointer userdata ) noexcept
  {
    assert( userdata != nullptr );

    slot_pointer  pSlot = memory_slot::slot_from_user_data(userdata);
    if ( (pSlot-_first_slot) < items )
    {
      pSlot->prt()->~value_type();

      _mtx_next.lock();

      pSlot->set_free( _next_free );
      _next_free = pSlot;
      --_used_slots;

      _mtx_next.unlock();
    }
  }

  /**
   * @brief Allocate memory for value_type() object and invoke
   *        constructur accordingly with parameters.
   * 
   *        Note: this function is NOT thread safe, so it must be
   *              used only when a single thread access to it or
   *              if synch occurs in the calling class. 
   * 
   *        Note: unsafe_ can be up to 40% faster if compared to thread safe method.
   *              
   * 
   * @param args    list of arguments can be also empty, in such 
   *                case the default constructor will be invoked.
   *                When specified one or more arguments they should 
   *                match with a constructor for value_type() or 
   *                an error will be generated ar compile-time.
   * @return value_type* to constructed object.
   */
  template< typename... Args >
  constexpr inline pointer    unsafe_allocate( Args&&... args ) noexcept
  { 
    slot_pointer pCurrSlot = _next_free;
    if ( pCurrSlot == nullptr )
    {
      return nullptr;
    }

    _next_free = pCurrSlot->next();
    pCurrSlot->set_in_use();
    ++_used_slots;

    return new(pCurrSlot->prt()) value_type( std::forward<Args>(args)... );
  }

  /**
   * @brief Deallocate memory for @param userdata pointer and invoke ~value_type().
   * 
   *        Note: this function is NOT thread safe, so it must be
   *              used only when a single thread access to it or
   *              if synch occurs in the calling class. 
   * 
   *        Note: unsafe_ can be up to 40% faster if compared to thread safe method.
   *  
   * @param userdata  pointer to user data previously allocated with allocate().
   */
  constexpr inline void       unsafe_deallocate( pointer userdata ) noexcept
  {
    assert( userdata != nullptr );

    slot_pointer  pSlot = memory_slot::slot_from_user_data(userdata);
    if ( (pSlot-_first_slot) < items )
    {
      pSlot->prt()->~value_type();

      pSlot->set_free( _next_free );
      _next_free = pSlot;
      --_used_slots;
    }
  }

private:
  slot_pointer        _first_slot;
  slot_pointer        _last_slot;
  slot_pointer        _next_free;
  
  size_type           _used_slots;

  mutable std::mutex  _mtx_next;
};

} // namespace LIB_VERSION 

}

#endif //LOCK_FREE_ARENA_ALLOCATOR_H