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

#ifndef LOCK_FREE_RING_BUFFER_H
#define LOCK_FREE_RING_BUFFER_H

#include <array>
#include <atomic>

//delete
#include<iostream>

namespace lock_free {

inline namespace LF_LIB_VERSION {

template<typename data_t, typename data_size_t, data_size_t items>
requires std::is_unsigned_v<data_size_t> && (std::is_same_v<data_size_t,uint32_t> || std::is_same_v<data_size_t,uint64_t>)
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

private:
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
  constexpr inline ring_buffer()
    : m_array(std::make_unique<std::array<slot_t, items>>()),
      m_ndxWrite( 0 ),m_ndxRead(0),m_counter(0)
  { }

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

    if ( (*m_array)[ndx].status.compare_exchange_weak( expected, slot_status_t::eBusyForRead, std::memory_order_release, std::memory_order_acquire ) )
    {
      data = (*m_array)[ndx].data;

      (*m_array)[ndx].status.store( slot_status_t::eEmpty, std::memory_order_release );
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

    if ( (*m_array)[ndx].status.compare_exchange_weak( expected, slot_status_t::eBusyForWrite, std::memory_order_release, std::memory_order_acquire ) )
    {
      (*m_array)[ndx].data = data;

      (*m_array)[ndx].status.store( slot_status_t::eFull, std::memory_order_release );
      ++m_counter;

      return true;
    }

    return false;
  }

private:
  std::unique_ptr<std::array<slot_t, items>>  m_array;
  
  std::atomic<size_type>                      m_ndxWrite;
  std::atomic<size_type>                      m_ndxRead;
  std::atomic<size_type>                      m_counter;
};

} // namespace LF_LIB_VERSION 

}

#endif // LOCK_FREE_RING_BUFFER_H