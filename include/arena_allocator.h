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

#include <vector>
#include <assert.h>

#include "core/memory_address.h"
#include "core/memory_allocators.h"
#include "core/fixed_lookup_table.h"

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
template< typename data_t, typename data_size_t, 
          data_size_t chunk_size = 1024, data_size_t initial_size = chunk_size, data_size_t size_limit = 0,
          data_size_t alloc_threshold = (chunk_size / 10),
          typename allocator_t = core::default_allocator<data_size_t>
        > 
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,u_int32_t> || std::is_same_v<data_size_t,u_int64_t>)
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
    { return _ptr_next.test_flag(core::memory_address<memory_slot,size_type>::address_flags::DESTROY); }

    /***/
    constexpr inline bool         is_free() const noexcept
    { return !_ptr_next.test_flag(core::memory_address<memory_slot,size_type>::address_flags::DESTROY); }

    /***/
    constexpr inline void         set_free( memory_slot* next_free ) noexcept
    { 
      _ptr_next.set_address( next_free ); 
      _ptr_next.unset_flag ( core::memory_address<memory_slot,size_type>::address_flags::DESTROY ); 
    }

    /***/
    constexpr inline void         set_in_use() noexcept
    { 
      _ptr_next.set_address( nullptr );
      _ptr_next.set_flag   ( core::memory_address<memory_slot,size_type>::address_flags::DESTROY ); 
    }
    
    /***/
    constexpr inline size_type    get_index() noexcept
    { return _ptr_next.get_counter(); }

    /***/
    constexpr inline void         set_index( const size_type& index ) noexcept
    { _ptr_next.set_counter( index ); }

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
    static_assert(decltype(_next_free )::is_always_lock_free);
    static_assert(decltype(_max_length)::is_always_lock_free);
    static_assert(decltype(_free_slots)::is_always_lock_free);
    static_assert(decltype(_capacity  )::is_always_lock_free);

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
  { return _max_length.load( std::memory_order_relaxed )-_free_slots.load( std::memory_order_relaxed ); }

  /**
   * @brief Return max lenght for this arena_allocator.
   * 
   * @return max number of items accordingly with @tparam items. 
   */
  constexpr inline size_type  max_length() const noexcept
  { return _max_length.load( std::memory_order_relaxed ); }

  /**
   * @brief  Retrieve the size in bytes currently allocated.
   * 
   * @return current size for the allocated buffer. 
   */
  constexpr inline size_type  capacity() const noexcept
  { return _capacity.load( std::memory_order_relaxed ); }

  /**
   * @brief Return the largest supported allocation size.
   */
  constexpr inline size_type  max_size() const noexcept
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
    if ( alloc_threshold > 0 ) [[likely]]
    {
      if ( _free_slots.load( std::memory_order_relaxed ) <= alloc_threshold ) 
      { _sem_th_alloc.release(); }
    }
    else if ( _next_free.load( std::memory_order_relaxed ) == nullptr ) [[unlikely]] // && ( alloc_threshold == 0 ) second part is implicit.
    { add_mem_chuck(); } 

    slot_pointer pCurrSlot = _next_free.load( std::memory_order_acquire );
    if ( pCurrSlot == nullptr ) [[unlikely]]
      return nullptr;

    do{
    } while ( (pCurrSlot) && !_next_free.compare_exchange_weak( pCurrSlot, pCurrSlot->next(), std::memory_order_acq_rel, std::memory_order_acquire ) );

    if ( pCurrSlot == nullptr )
      return nullptr;

    pCurrSlot->set_in_use();

    size_type _cur_value = _free_slots.load( std::memory_order_acquire );
    do {
    } while ( !_free_slots.compare_exchange_weak( _cur_value, _cur_value-1, std::memory_order_acq_rel, std::memory_order_acquire ) );     
    
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
  constexpr inline void       deallocate( pointer userdata ) noexcept
  {
    assert( userdata != nullptr );

    userdata->~value_type();

    slot_pointer     pSlot     = memory_slot::slot_from_user_data(userdata);
    arena_allocator* pArena    = instances_table[pSlot->get_index()];
    slot_pointer     pCurrNext = pArena->_next_free.load( std::memory_order_acquire );

    do{
      pSlot->set_free( pCurrNext );
    } while ( !_next_free.compare_exchange_weak( pCurrNext, pSlot, std::memory_order_acq_rel, std::memory_order_acquire ) );
    
    size_type _cur_value = _free_slots.load( std::memory_order_acquire );
    do {
    } while ( !_free_slots.compare_exchange_weak( _cur_value, _cur_value+1, std::memory_order_acq_rel, std::memory_order_acquire ) );    
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
    if (( _next_free.load( std::memory_order_relaxed ) == nullptr ) && ( alloc_threshold == 0 ))
    { unsafe_add_mem_chuck(); } 

    slot_pointer pCurrSlot = _next_free.load( std::memory_order_relaxed );
    if ( pCurrSlot == nullptr )
      return nullptr;

    _next_free.exchange( pCurrSlot->next(), std::memory_order_relaxed );

    pCurrSlot->set_in_use();

    _free_slots.fetch_sub( 1, std::memory_order_relaxed );

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
  constexpr inline void       unsafe_deallocate( pointer userdata ) noexcept
  {
    assert( userdata != nullptr );

    userdata->~value_type();

    slot_pointer  pSlot = memory_slot::slot_from_user_data(userdata);

    pSlot->set_free( _next_free.load( std::memory_order_relaxed ) );
    
    _next_free.exchange( pSlot, std::memory_order_relaxed );

    _free_slots.fetch_add( 1, std::memory_order_relaxed );
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
    if ( userdata == nullptr )
      return false;

    using addr_base_type = core::memory_address<memory_slot,size_type>::base_t;

    const addr_base_type addr_offset = core::memory_address<memory_slot,size_type>::memory_address_size;

    const std::lock_guard<std::mutex> lock(_mtx_mem_chunks);
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

    using addr_base_type = core::memory_address<memory_slot,size_type>::base_t;

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
    _max_length.store(       0, std::memory_order_relaxed );
    _free_slots.store(       0, std::memory_order_relaxed );
    _capacity.store  (       0, std::memory_order_relaxed );
    _next_free.store ( nullptr, std::memory_order_relaxed );  
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

    slot_pointer pCurrNext = _next_free.load( std::memory_order_acquire );
    do
    {
      _new_mem_chunck._last_slot->set_free( pCurrNext );

    } while ( !_next_free.compare_exchange_weak( pCurrNext, _new_mem_chunck._first_slot, std::memory_order_release, std::memory_order_relaxed ) );
    
    // Protect access to _next_free
    _mtx_mem_chunks.lock();

        /////////////////////
        // Store chunck information in a vector.
        _mem_chunks.push_back(_new_mem_chunck);
        
        // Update max_length
        _max_length.store( chunk_size*_mem_chunks.size(), std::memory_order_relaxed );
        _free_slots.fetch_add(chunk_size, std::memory_order_relaxed);

        // Update capacity
        _capacity.store( memory_required_per_chunk*_mem_chunks.size(), std::memory_order_relaxed );

    // Release _next_free mutex
    _mtx_mem_chunks.unlock();

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
    _max_length.store( chunk_size*_mem_chunks.size(), std::memory_order_relaxed );
    _free_slots.fetch_add(chunk_size, std::memory_order_relaxed);

    // Update capacity
    _capacity.store( memory_required_per_chunk*_mem_chunks.size(), std::memory_order_relaxed );

    return true;
  }

  /***/
  void print_internal_status()
  { 
    std::cout << "-----------------BEGIN-----------------" << std::endl;
    std::cout << "_next_free  = " << _next_free.load()  << std::endl;
    std::cout << "_max_length = " << _max_length.load() << std::endl;
    std::cout << "_free_slots = " << _free_slots.load() << std::endl;
    std::cout << "_capacity   = " << _capacity.load()   << std::endl;
    
    for ( size_type ndx = 0; ndx < _mem_chunks.size(); ++ndx )
    {
      memory_chunk& mc = _mem_chunks.at(ndx);

      std::cout << "[" << ndx << "] _first_slot = " << mc._first_slot << std::endl;
      std::cout << "[" << ndx << "] _last_slot  = " << mc._last_slot  << std::endl;
    }
    std::cout << "------------------END------------------" << std::endl;
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

  std::atomic<slot_pointer>   _next_free;
  std::atomic<size_type>      _max_length;
  std::atomic<size_type>      _free_slots;
  std::atomic<size_type>      _capacity;

  std::mutex                  _mtx_mem_chunks;

  std::thread*                _th_alloc;
  std::binary_semaphore       _sem_th_alloc; 
  std::atomic_bool            _th_alloc_exit;
};

template< typename data_t, typename data_size_t, data_size_t chunk_size, data_size_t initial_size, data_size_t size_limit,
          data_size_t alloc_threshold, typename allocator_t >
arena_allocator<data_t,data_size_t,chunk_size,initial_size,size_limit,alloc_threshold,allocator_t>::lookup_table_type          
  arena_allocator<data_t,data_size_t,chunk_size,initial_size,size_limit,alloc_threshold,allocator_t>::instances_table;

} // namespace LIB_VERSION 

}

#endif //LOCK_FREE_ARENA_ALLOCATOR_H
