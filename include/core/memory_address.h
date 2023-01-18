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

#ifndef CORE_MEMORY_ADDRESS_H
#define CORE_MEMORY_ADDRESS_H

#include <bit>
#include <type_traits>
#include <cstddef>
#include <cstdint>

#include "config.h"
#include "types.h"

namespace core {

inline namespace LF_LIB_VERSION {

/**
 * @brief A data structure used as wrapper for a pointer.
 *        On 64 bits architeture leverage unused bits for different usage.
 *        On 32 bits since all bits are used then a new variable is declared.
 * 
 * @tparam data_t       data type specialization for the template 
 * @tparam data_size_t  data type to be used for numeric values.
 */
template< typename data_t, typename data_size_t>
requires std::is_unsigned_v<data_size_t> 
         && ((sizeof(data_t*)==4) || (sizeof(data_t*)==8))
struct memory_address {
public:  
  using value_type      = data_t;
  using size_type       = data_size_t;
  using pointer         = value_type*;
  using const_pointer   = const value_type*;

  static constexpr const size_type memory_address_size = sizeof(memory_address<value_type,size_type>);

private:


public:
  /** 
   * @brief Define base_t as uint64_t or uint32_t accordingly with pointer size.
   *        Note that due to restriction applied to memory_address structure only 32bits 
   *        and 64bits architectures are supported.
   */
  typedef std::conditional_t< (sizeof(pointer)==8), uint64_t, uint32_t >  base_t;

  enum class address_flags : base_t {
    DESTROY = 0x0001
  };

  /***/
  constexpr inline memory_address() noexcept
    : _addr( std::bit_cast<base_t>(nullptr) ), _flags(0), _counter(0)
  {}
  /***/
  constexpr inline explicit memory_address( pointer ptr ) noexcept
    : _addr( std::bit_cast<base_t>(ptr) ), _flags(0), _counter(0)
  {}  
  /***/
  constexpr inline memory_address( pointer ptr, base_t flags, base_t counter ) noexcept
    : _addr( std::bit_cast<base_t>(ptr) ), _flags(flags), _counter(counter)
  {}

  /***/
  static constexpr inline void   copy_from( memory_address<value_type,size_type>& to, const memory_address<value_type,size_type>& from ) noexcept
  {
    to._addr    = from._addr;
    to._flags   = from._flags;
    to._counter = from._counter;
  }

  /***/
  constexpr inline void   reset( pointer ptr, base_t flags = 0, base_t counter = 0 ) noexcept
  {
    _addr    = std::bit_cast<base_t>(ptr);
    _flags   = flags;
    _counter = counter;
  }

  /***/  
  constexpr inline operator pointer() const noexcept
  { return std::bit_cast<pointer>(_addr); }
  
  /***/  
  constexpr inline pointer  operator->() const noexcept
  { return std::bit_cast<pointer>(_addr); }

  /***/
  constexpr inline pointer  get_address() const noexcept 
  { return std::bit_cast<pointer>(_addr); }

  /***/
  constexpr inline void     set_address( pointer ptr ) noexcept
  { _addr    = std::bit_cast<base_t>(ptr); }

  /***/
  static constexpr inline base_t   flags( const memory_address<value_type,size_type>& obj ) noexcept 
  { return obj._flags; }

  /***/
  static constexpr inline bool     test_flag( const memory_address<value_type,size_type>& obj, address_flags flag ) noexcept 
  { return (obj._flags & (base_t)flag); }

  /***/
  static constexpr inline void     set_flag( memory_address<value_type,size_type>& obj, address_flags flag ) noexcept
  { obj._flags |= (base_t)flag; }

  /***/
  static constexpr inline void     unset_flag( memory_address<value_type,size_type>& obj, address_flags flag ) noexcept
  { obj._flags &= ~(base_t)flag; }

  /***/
  static constexpr inline void     unset_all( memory_address<value_type,size_type>& obj ) noexcept
  { obj._flags = 0; }
  
  /***/
  static constexpr inline base_t   get_counter( const memory_address<value_type,size_type>& obj ) noexcept 
  { return obj._counter; }

  /***/
  static constexpr inline void     set_counter( memory_address<value_type,size_type>& obj, base_t counter ) noexcept 
  { obj._counter = counter; }

  /***/
  static constexpr inline void     add_counter( memory_address<value_type,size_type>& obj, base_t value ) noexcept 
  { obj._counter += value; }
  /***/
  static constexpr inline void     sub_counter( memory_address<value_type,size_type>& obj, base_t value ) noexcept 
  { obj._counter -= value; }

private:
  base_t   _addr    : conditional<size_type,(sizeof(pointer)==8), 48, 32>::value;
  base_t   _flags   : conditional<size_type,(sizeof(pointer)==8),  4,  4>::value;
  base_t   _counter : conditional<size_type,(sizeof(pointer)==8), 12, 28>::value;
};

template< typename data_t, typename data_size_t>
requires std::is_unsigned_v<data_size_t> 
         && ((sizeof(data_t*)==4) || (sizeof(data_t*)==8))
class double_memory_address
{
public:  
  using value_type      = data_t;
  using size_type       = data_size_t;
  using pointer         = value_type*;
  using addreess_type   = memory_address<value_type,size_type>;

  static constexpr const size_type memory_address_size = sizeof(double_memory_address<value_type,size_type>);

  /***/
  constexpr inline double_memory_address() noexcept
    : _addr1(), _addr2()
  {}
  /***/
  constexpr inline double_memory_address( pointer ptr1, pointer ptr2 ) noexcept
    : _addr1(ptr1), _addr2(ptr2)
  { }
  /***/
  constexpr inline double_memory_address( const addreess_type& addr1, const addreess_type& addr2 ) noexcept
  {
    _addr1.copy_from( addr1 ) ;
    _addr2.copy_from( addr2 ) ;
  }

  /***/
  constexpr inline addreess_type& get_addr1() noexcept
  { return _addr1; }
  /***/
  constexpr inline addreess_type& get_addr2() noexcept
  { return _addr2; }

protected:
  addreess_type  _addr1;
  addreess_type  _addr2;

};

} // namespace LF_LIB_VERSION 

}

#endif // CORE_MEMORY_ADDRESS_H
