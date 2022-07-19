#ifndef LOCK_FREE_RING_BUFFER_H
#define LOCK_FREE_RING_BUFFER_H

#include <array>
#include <atomic>

//delete
#include<iostream>

namespace lock_free {

template<typename data_t, typename data_size_t, data_size_t items>
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,u_int32_t> || std::is_same_v<data_size_t,u_int64_t>)
class ring_buffer
{
public:
  using value_type      = data_t; 
  using size_type       = data_size_t;
  using reference       = data_t&;
  using const_reference = const data_t&;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;

  static constexpr const size_type data_type_size = sizeof(value_type);

public:
  enum class slot_status_t : size_type {
    eEmpty = 0,
    eBusyForRead,
    eBusyForWrite,
    eFull
  };

  struct slot_t
  {
    slot_t()
     : status(slot_status_t::eEmpty)
    {}

    std::atomic<slot_status_t>    status;
    value_type                    data;
  };

public:
  /***/
  constexpr ring_buffer()
    : m_ndxWrite( 0 ),m_ndxRead(0),m_counter(0)
  {
    static_assert( m_ndxWrite.is_always_lock_free );
    static_assert( m_ndxRead.is_always_lock_free  );
    static_assert( m_counter.is_always_lock_free  );
  }

  /***/
  constexpr inline size_type size() const noexcept
  { return m_counter.load(std::memory_order_relaxed); }
 
  /**
   * @brief This method will be specialized for both rvalue or lvalue.
   * 
   * @tparam value_type 
   * @param data 
   * @return true 
   * @return false 
   */
  template<typename value_type>
  constexpr inline bool      push( value_type&& data ) noexcept
  {
    return _push( std::forward<value_type>(data) );
  }
 
  /***/
  constexpr inline bool      pop( value_type& data ) noexcept
  {
    size_type     ndx      = m_ndxRead.fetch_add( 1, std::memory_order_seq_cst );
    if ( ndx > items)
      return false;

    if ( ndx == items )
    {
      ndx = 0;
      m_ndxRead.store( 1, std::memory_order_release );
    }

    slot_status_t expected = slot_status_t::eFull;

    if ( m_array[ndx].status.compare_exchange_weak( expected, slot_status_t::eBusyForRead, std::memory_order_release, std::memory_order_acquire ) )
    {
      data = m_array[ndx].data;

      m_array[ndx].status.store( slot_status_t::eEmpty, std::memory_order_release );
      --m_counter;

      return true;
    } 

    return false;
  }

protected:

private:
  /***/
  template<typename value_type>
  constexpr inline bool _push( value_type&& data ) noexcept
  {
    size_type     ndx      = m_ndxWrite.fetch_add( 1, std::memory_order_seq_cst );
    if ( ndx > items)
      return false;

    if ( ndx == items )
    {
      ndx = 0;
      m_ndxWrite.store( 1, std::memory_order_release );
    }

    slot_status_t expected = slot_status_t::eEmpty;

    if ( m_array[ndx].status.compare_exchange_weak( expected, slot_status_t::eBusyForWrite, std::memory_order_release, std::memory_order_acquire ) )
    {
      m_array[ndx].data = data;

      m_array[ndx].status.store( slot_status_t::eFull, std::memory_order_release );
      ++m_counter;

      return true;
    }

    return false;
  }

private:
  std::array<slot_t, items>  m_array;
  
  std::atomic<size_type>     m_ndxWrite;
  std::atomic<size_type>     m_ndxRead;
  std::atomic<size_type>     m_counter;
};

}

#endif // LOCK_FREE_RING_BUFFER_H