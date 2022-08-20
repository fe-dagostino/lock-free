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

#ifndef CORE_ARENA_ALLOCATOR_H
#define CORE_ARENA_ALLOCATOR_H

#include <vector>
#include <assert.h>

#include "mutex.h"
#include "memory_address.h"
#include "memory_allocators.h"
#include "fixed_lookup_table.h"

namespace core {

inline namespace LIB_VERSION {

/***
 * An arena_allocator implementation that keep both alloc() and dealloc() to a complexity of O(1).
 * 
 * This arena_allocator is useful when :
 *  - in a program there is an extensive use of "new" and "delete" for a well-defined "data type";
 *  - we want to avoid memory fragmentation, since the program is supposed to run for long time without interruptions;
 *  - performances in our program are important and we want to avoid waste of CPU cycles.
 * 
 * @tparam data_t           data type held by the queue
 * @tparam data_size_t      data type to be used internally for counting and sizing. 
 *                          This is required to be 32 bits or 64 bits in order to keep performances.
 * @tparam chunk_size       number of data_t items to pre-alloc each time that is needed.
 * @tparam initial_size     reserved size for the queue, this size will be reserved when the object is created.
 *                          When application level know the amount of memory items to be used this parameter allow
 *                          to improve significantly performances as well as to avoid fragmentation.
 * @tparam size_limit       default value is 0 that means the queue can grow until there is available memory.
 *                          A value different greater than 0 will have the effect to limit max number of items on 
 *                          the queue.
 * @tparam alloc_threshold  when 0 chunck allocation is synchronous, when >0 specify the threshold to activate
 *                          an auxiliare thread to add a new chunck. 
 * @tparam allocator_t      system memory allocator core::default_allocator, core::virtual_allocator or user defined allocator.
 * 
*/
template< typename data_t, typename data_size_t, 
          data_size_t chunk_size = 1024, data_size_t initial_size = chunk_size, data_size_t size_limit = 0,
          data_size_t alloc_threshold = (chunk_size / 10),
          typename allocator_t = core::default_allocator<data_size_t>
        > 
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,uint32_t> || std::is_same_v<data_size_t,uint64_t>)
         && ( ((sizeof(data_t) % alignof(std::max_align_t)) == 0 ) || ((alignof(std::max_align_t) % sizeof(data_t)) == 0 ) )
         && ( chunk_size > 0 ) && ( initial_size >= chunk_size )
         && ((sizeof(void*)==4) || (sizeof(void*)==8))
class arena_allocator
{
private:
  static constexpr const data_size_t        max_instances_per_type = 1024;

public:
  using value_type        = data_t; 
  using size_type         = data_size_t;
  using pointer           = data_t*;
  using const_pointer     = const data_t*;
  using allocator_type    = allocator_t;
  using lookup_table_type = core::fixed_lookup_table<arena_allocator*,size_type,max_instances_per_type,nullptr>;
  using mutex_type        = core::mutex;

  static constexpr const size_type         value_type_size  = sizeof(value_type);
  static lookup_table_type                 instances_table;

private:
  /***/
  struct memory_slot {
    /***/
    constexpr inline memory_slot() noexcept
      :_ptr_next(nullptr)
    {}
    /***/
    constexpr inline explicit memory_slot( memory_slot* next ) noexcept
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
    { return core::memory_address<memory_slot,size_type>::test_flag( _ptr_next, core::memory_address<memory_slot,size_type>::address_flags::DESTROY); }

    /***/
    constexpr inline bool         is_free() const noexcept
    { return !core::memory_address<memory_slot,size_type>::test_flag( _ptr_next, core::memory_address<memory_slot,size_type>::address_flags::DESTROY); }

    /***/
    constexpr inline void         set_free( memory_slot* next_free ) noexcept
    { 
      _ptr_next.set_address( next_free ); 
      core::memory_address<memory_slot,size_type>::unset_flag ( _ptr_next, core::memory_address<memory_slot,size_type>::address_flags::DESTROY ); 
    }

    /***/
    constexpr inline void         set_in_use() noexcept
    { 
      _ptr_next.set_address( nullptr );
      core::memory_address<memory_slot,size_type>::set_flag   ( _ptr_next, core::memory_address<memory_slot,size_type>::address_flags::DESTROY ); 
    }
    
    /***/
    constexpr inline size_type    get_index() noexcept
    { return core::memory_address<memory_slot,size_type>::get_counter( _ptr_next ); }

    /***/
    constexpr inline void         set_index( const size_type& index ) noexcept
    { core::memory_address<memory_slot,size_type>::set_counter( _ptr_next, index ); }

    /***/
    constexpr static inline memory_slot* slot_from_user_data( pointer ptr ) noexcept
    { return std::bit_cast<memory_slot*>(std::bit_cast<char*>(ptr)-core::memory_address<memory_slot,size_type>::memory_address_size); }

    core::memory_address<memory_slot,size_type>  _ptr_next;
    value_type                                   _user_data;
  };

  using slot_pointer     = memory_slot*;
  
  static constexpr const size_type memory_slot_size           = sizeof(memory_slot);
  static constexpr const size_type memory_required_per_chunk  = memory_slot_size*chunk_size;

public:

  /***/
  constexpr inline arena_allocator() noexcept
    : _ndx_instance( 0 ),
      _next_free(nullptr), _max_length(0), _free_slots(0), _capacity(0),
      _th_alloc(nullptr), _sem_th_alloc(0), _th_alloc_exit( false )
  {
    capture_instance_index();

    while ( max_length() < initial_size )
    {
      if ( add_mem_chuck() == false )
      { break; }
    }

    if ( alloc_threshold > 0 )
    {
      _th_alloc = new std::thread( []( arena_allocator* pArena ){ 
                                                                  while ( pArena->_th_alloc_exit.load(std::memory_order_acquire) == false )
                                                                  {
                                                                    pArena->_sem_th_alloc.acquire();

                                                                    if ( pArena->_th_alloc_exit.load(std::memory_order_acquire) == true  )
                                                                      break;

                                                                    if (( pArena->max_length() < size_limit ) || (size_limit == 0))
                                                                    {
                                                                      if ( pArena->add_mem_chuck( ) == false )
                                                                      {
                                                                        // Reached physical memory limit
                                                                      }
                                                                    }
                                                                  }         
                                                                }, this );
    }
  }

  /***/
  constexpr inline arena_allocator( const arena_allocator& ft ) noexcept = delete;

  /***/
  constexpr inline ~arena_allocator() noexcept
  {
    release_instance_index();

    if ( alloc_threshold > 0 )
    {
      _th_alloc_exit.store( true, std::memory_order_release );
      _sem_th_alloc.release();
    }

    // Release all chunks
    clear();

    if (( alloc_threshold > 0 ) && (_th_alloc!=nullptr))
    {
      _th_alloc->join();
      delete _th_alloc;
    }
  }

  /***/
  constexpr inline arena_allocator& operator=( const arena_allocator& ft ) noexcept = delete;
  /***/
  constexpr inline arena_allocator& operator=( const arena_allocator&& ft ) noexcept = delete;

  /**
   * @brief Retrieve length in bytes for one single data_t.
   * 
   * @return sizeof(data_t) 
   */
  static constexpr inline size_type  type_size() noexcept
  { return value_type_size; }

  /**
   * @brief Retrieve length in terms of items currently used.
   * 
   * @return current number of items in use. 
   */
  constexpr inline size_type  length() const noexcept
  { 
    _mtx_next.lock();
    size_type ret_val = (_max_length-_free_slots);
    _mtx_next.unlock();
    return ret_val; 
  }

  /**
   * @brief Retrieve length in terms of items currently in the buffer.
   * 
   * @return current items in the data buffer. 
   */
  constexpr inline size_type  unsafe_length() const noexcept
  { return (_max_length-_free_slots); }

  /**
   * @brief Return max lenght for this arena_allocator.
   * 
   * @return max number of items accordingly with @tparam items. 
   */
  constexpr inline size_type  max_length() const noexcept
  { 
    _mtx_next.lock();
    size_type ret_val = _max_length;
    _mtx_next.unlock();
    return ret_val; 
  }

  /***/
  constexpr inline size_type  unsafe_max_length() const noexcept
  { return _max_length; }

  /**
   * @brief  Retrieve the size in bytes currently allocated.
   * 
   * @return current size for the allocated buffer. 
   */
  constexpr inline size_type  capacity() const noexcept
  { 
    _mtx_next.lock();
    size_type ret_val = _capacity;
    _mtx_next.unlock();
    return ret_val;     
  }

  /**
   * @brief Return the largest supported allocation size.
   */
  static constexpr inline size_type  max_size() noexcept
  { return std::numeric_limits<size_type>::max() / memory_slot_size; }
  
  /**
   * @brief Allocate memory for data_t() object and invoke
   *        constructur accordingly with parameters.
   * 
   *        Note: this function is thread safe, if you are in single
   *              thread context evaluate to use unsafe_allocate() instead.
   * 
   * @param args    list of arguments can be also empty, in such 
   *                case the default constructor will be invoked.
   *                When specified one or more arguments they should 
   *                match with a constructor for data_t() or 
   *                an error will be generated ar compile-time.
   * @return data_t* to constructed object. Function may return nullptr in
   *         the following circumstances:
   *         1) not enough memory to allocate a new memory_chunk
   *         2) not available memory_slot since the max_size has been 
   *            limited using size_limit template parameter.
   *         3) service thread is allocating a new memory_chunk or
   *            a different working thread is allocating a memory_cunck,
   *            this is behaviour is regulated bu alloc_threshold 
   *            template parameter. 
   */
  template< typename... Args > 
  constexpr inline pointer    allocate( Args&&... args ) noexcept
  { 
    do{
    } while ( !_mtx_next.try_lock() );

      if ( alloc_threshold > 0 ) [[likely]]
      {
        if ( _free_slots <= alloc_threshold ) 
        { _sem_th_alloc.release(); }
      }
      else if ( _next_free == nullptr ) [[unlikely]] // && ( alloc_threshold == 0 ) second part is implicit.
      { unsafe_add_mem_chuck(); } 

      slot_pointer pCurrSlot = _next_free;
      if ( pCurrSlot == nullptr ) [[unlikely]]
      {
        _mtx_next.unlock();
        return nullptr;
      }

      _next_free = pCurrSlot->next();
      --_free_slots;
    
    _mtx_next.unlock();
    
    pCurrSlot->set_in_use();

    return new(pCurrSlot->prt()) value_type( std::forward<Args>(args)... );
  }

  /**
   * @brief Deallocate memory for @param userdata pointer and invoke ~data_t().
   * 
   *        Note: this function is thread safe, if you are in single
   *              thread context evaluate to use unsafe_allocate() instead.
   * 
   *        Note: only memory allocated by this arena_allocator can be deallocated.
   *              In case the user try to deallocate memory not managed from the arena_allocator
   *              application will experience unexpected behaviour. Consistency check require O(n) 
   *              where 'n' is the number of chunks allocated, so not implemented in order to have
   *              deallocation with O(1).
   *  
   * @param userdata  pointer to user data previously allocated with allocate().
   */
  [[nodiscard]] static constexpr inline result_t deallocate( pointer userdata ) noexcept
  {
    if ( userdata == nullptr )
      return core::result_t::eNullPointer;

    userdata->~value_type();

    slot_pointer     pSlot     = memory_slot::slot_from_user_data(userdata);
    arena_allocator* pArena    = instances_table[pSlot->get_index()];

    if ( pSlot->is_free() )
    {
      // double free detected 
      return result_t::eDoubleFree;
    }

    do{
    } while ( !pArena->_mtx_next.try_lock() );

      pSlot->set_free( pArena->_next_free );
      pArena->_next_free = pSlot;
      ++pArena->_free_slots;

    pArena->_mtx_next.unlock();

    return result_t::eSuccess;
  }

  /**
   * @brief Allocate memory for data_t() object and invoke
   *        constructur accordingly with parameters.
   * 
   *        Note: this function is NOT thread safe, so it must be
   *              used only when a single thread access to it or
   *              if synch occurs in the calling class. 
   * 
   *        Note: unsafe_ can be up to 40% faster if compared to thread safe method.
   *     
   * @param args    list of arguments can be also empty, in such 
   *                case the default constructor will be invoked.
   *                When specified one or more arguments they should 
   *                match with a constructor for data_t() or 
   *                an error will be generated ar compile-time.
   * 
   * @return data_t* to constructed object. Function may return nullptr in
   *         the following circumstances:
   *         1) not enough memory to allocate a new memory_chunk
   *         2) not available memory_slot since the max_size has been 
   *            limited using size_limit template parameter.
   *         3) service thread is allocating a new memory_chunk or
   *            a different working thread is allocating a memory_cunck,
   *            this is behaviour is regulated bu alloc_threshold 
   *            template parameter. 
   */
  template< typename... Args >
  constexpr inline pointer    unsafe_allocate( Args&&... args ) noexcept
  { 
    if (( _next_free == nullptr ) && ( alloc_threshold == 0 ))
    { unsafe_add_mem_chuck(); } 

    slot_pointer pCurrSlot = _next_free;
    if ( pCurrSlot == nullptr )
      return nullptr;

    _next_free = pCurrSlot->next();
    pCurrSlot->set_in_use();
    --_free_slots;

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
   *        Note: only memory allocated by this arena_allocator can be deallocated.
   *              In case the user try to deallocate memory not managed from the arena_allocator
   *              application will experience unexpected behaviour. Consistency check require O(n) 
   *              where 'n' is the number of chunks allocated, so not implemented in order to have
   *              deallocation with O(1).
   *   
   * @param userdata  pointer to user data previously allocated with allocate().
   */
  [[nodiscard]] static constexpr inline result_t unsafe_deallocate( pointer userdata ) noexcept
  {
    if ( userdata == nullptr )
      return core::result_t::eNullPointer;

    userdata->~value_type();

    slot_pointer     pSlot  = memory_slot::slot_from_user_data(userdata);
    arena_allocator* pArena = instances_table[pSlot->get_index()];

    if ( pSlot->is_free() )
    {
      // double free detected 
      return result_t::eDoubleFree;
    }

    pSlot->set_free( pArena->_next_free );

    pArena->_next_free = pSlot;

    ++pArena->_free_slots;

    return result_t::eSuccess;
  }

  /**
   * @brief Check if specified pointer is managed by arena_allocator.
   * 
   * @param userdata pointer to user data previously returned by allocate() or unsafe_allocate().
   * @return true    if memory address is in the range managed by arena_allocator instance
   * @return false   if memory address is do not fit in any allocated memory chunk
   */
  [[nodiscard]] constexpr inline bool is_valid( pointer userdata ) const
  {
    core::lock_guard<mutex_type> mtx(_mtx_next);
    
    return unsafe_is_valid( userdata );
  }

  /**
   * @brief Check if specified pointer is managed by arena_allocator.
   * 
   * @param userdata pointer to user data previously returned by allocate() or unsafe_allocate().
   * @return true    if memory address is in the range managed by arena_allocator instance
   * @return false   if memory address is do not fit in any allocated memory chunk
   */
  [[nodiscard]] constexpr inline bool unsafe_is_valid( pointer userdata ) const
  {
    if ( userdata == nullptr )
      return false;

    using addr_base_type = typename core::memory_address<memory_slot,size_type>::base_t;

    const addr_base_type addr_offset = core::memory_address<memory_slot,size_type>::memory_address_size;

    for ( auto& mc : _mem_chunks )
    {
      if (
          ( std::bit_cast<addr_base_type>(userdata) >= std::bit_cast<addr_base_type>(mc._first_slot)+addr_offset ) && 
          ( std::bit_cast<addr_base_type>(userdata) <= std::bit_cast<addr_base_type>(mc._last_slot )+addr_offset )
         )
         return true;
    }

    return false;
  }

  /**
   * @brief Invoke valut_type destructor for each slot that is in use,
   *        then release all the memory associated with the chuncks.
   *        After the call to this methos the allocator will completely 
   *        empty and no memroy chunck are reserved.
   * 
   *        Note: this method is not thread safe.
   */
  constexpr inline void clear() noexcept
  {
    for ( auto& mc : _mem_chunks )
    {
      // Iterate on overall slots in order to invoke ~value_type() 
      // for all object currently in use.
      slot_pointer mem_curs = mc._first_slot;
      size_type    slots_nb = 0; 
  
      while ( slots_nb++ < chunk_size )
      {
        if ( mem_curs->in_use() == true )
          mem_curs->prt()->~value_type();

        mem_curs++;
      }
  
      // Release memory allocated in the in the constructor.
      _mem_allocator.deallocate( mc._first_slot, memory_required_per_chunk );
      
      mc.reset();
    }
    _mem_chunks.clear();
    
    // Update max_length
    _max_length = 0;
    _free_slots = 0;
    _capacity   = 0;      
    _next_free  = nullptr;
  }

private:
  /***/
  constexpr inline bool add_mem_chuck() noexcept
  {
    memory_chunk _new_mem_chunck;
   
    _new_mem_chunck._first_slot = static_cast<slot_pointer>(_mem_allocator.allocate( memory_required_per_chunk ));
    if ( _new_mem_chunck._first_slot == nullptr )
    { return false; }

    _new_mem_chunck._last_slot  = _new_mem_chunck._first_slot+(chunk_size-1);

    // Iterate on overall slots in order to initialize the slots chain.
    slot_pointer mem_curs = _new_mem_chunck._first_slot;
    size_type    slots_nb = chunk_size; 
    while ( slots_nb-- )
    {
      mem_curs->set_index( _ndx_instance );
      mem_curs->set_free ((slots_nb>0)?(mem_curs+1):nullptr);

      mem_curs++;
    }

    // Protect access to _next_free
    do{
    } while ( !_mtx_next.try_lock() );

      _new_mem_chunck._last_slot->set_free( _next_free );

      _next_free  = _new_mem_chunck._first_slot; // next free item initialized with first item._

      /////////////////////
      // Store chunck information in a vector.
      _mem_chunks.push_back(_new_mem_chunck);

      // Update max_length
      _max_length  = chunk_size*_mem_chunks.size();
      _free_slots += chunk_size;

      // Update capacity
      _capacity    = memory_required_per_chunk*_mem_chunks.size();      

    // Release _next_free mutex
    _mtx_next.unlock();

    return true;
  }

  /***/
  constexpr inline bool unsafe_add_mem_chuck() noexcept
  {
    memory_chunk _new_mem_chunck;
   
    _new_mem_chunck._first_slot = static_cast<slot_pointer>(_mem_allocator.allocate( memory_required_per_chunk ));
    if ( _new_mem_chunck._first_slot == nullptr )
    { return false; }

    _new_mem_chunck._last_slot  = _new_mem_chunck._first_slot+(chunk_size-1);

    // Iterate on overall slots in order to initialize the slots chain.
    slot_pointer mem_curs = _new_mem_chunck._first_slot;
    size_type    slots_nb = chunk_size; 
    while ( slots_nb-- )
    {
      mem_curs->set_index( _ndx_instance );
      mem_curs->set_free((slots_nb>0)?(mem_curs+1):nullptr);

      mem_curs++;
    }

    _new_mem_chunck._last_slot->set_free( _next_free );

    _next_free  = _new_mem_chunck._first_slot; // next free item initialized with first item._

    /////////////////////
    // Store chunck information in a vector.
    _mem_chunks.push_back(_new_mem_chunck);

    // Update max_length
    _max_length  = chunk_size*_mem_chunks.size();
    _free_slots += chunk_size; 

    // Update capacity
    _capacity    = memory_required_per_chunk*_mem_chunks.size();      

    return true;
  }

  /***/
  constexpr inline void  capture_instance_index()
  {
    [[maybe_unused]] bool ret_val = instances_table.add(_ndx_instance, this );
    assert( ret_val == true );
  }

  /***/
  constexpr inline void  release_instance_index()
  {
    [[maybe_unused]] bool ret_val = instances_table.reset_at(_ndx_instance);
    assert( ret_val == true );
  }

private:
  /***/
  struct memory_chunk {
    constexpr inline memory_chunk() noexcept
      : _first_slot(nullptr), _last_slot(nullptr)
    {}
    /* Set both pointer to nullptr */
    constexpr inline void reset() noexcept
    {
      _first_slot = nullptr;
      _last_slot  = nullptr;
    }

    slot_pointer     _first_slot;
    slot_pointer     _last_slot;
  };

  size_type                   _ndx_instance;

  allocator_type              _mem_allocator;
  std::vector<memory_chunk>   _mem_chunks;

  slot_pointer                _next_free;
  size_type                   _max_length;
  size_type                   _free_slots;
  size_type                   _capacity;

  mutable mutex_type          _mtx_next;

  std::thread*                _th_alloc;
  std::binary_semaphore       _sem_th_alloc; 
  std::atomic_bool            _th_alloc_exit;
};

template< typename data_t, typename data_size_t, data_size_t chunk_size, data_size_t initial_size, data_size_t size_limit,
          data_size_t alloc_threshold, typename allocator_t >
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,uint32_t> || std::is_same_v<data_size_t,uint64_t>)
         && ( ((sizeof(data_t) % alignof(std::max_align_t)) == 0 ) || ((alignof(std::max_align_t) % sizeof(data_t)) == 0 ) )
         && ( chunk_size > 0 ) && ( initial_size >= chunk_size )
         && ((sizeof(void*)==4) || (sizeof(void*)==8))
typename arena_allocator<data_t,data_size_t,chunk_size,initial_size,size_limit,alloc_threshold,allocator_t>::lookup_table_type          
  arena_allocator<data_t,data_size_t,chunk_size,initial_size,size_limit,alloc_threshold,allocator_t>::instances_table;

} // namespace LIB_VERSION 

}

#endif //CORE_ARENA_ALLOCATOR_H
