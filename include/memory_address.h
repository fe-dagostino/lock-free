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

namespace core {

inline namespace LIB_VERSION {

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
  using pointer         = data_t*;
  using const_pointer   = const data_t*;

  static constexpr const size_type memory_address_size = sizeof(memory_address<value_type,size_type>);

private:
  /**
   * @brief Similar to conditional_t, the same pattern have been applied to 
   *        values.
   */
  template<bool B, size_type T, size_type F>
  struct conditional              { enum : size_type { value = T }; };
  /* specialization for 'false' */
  template<size_type T, size_type F>
  struct conditional<false, T, F> { enum : size_type { value = F }; };

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
    : _addr( std::bit_cast<base_t>(nullptr) ), _flags(0)
  {}
  /***/
  constexpr inline memory_address( pointer ptr ) noexcept
    : _addr( std::bit_cast<base_t>(ptr) ), _flags(0)
  {}  
  /***/
  constexpr inline memory_address( pointer ptr, base_t flags ) noexcept
    : _addr( std::bit_cast<base_t>(ptr) ), _flags(flags)
  {}

  /***/  
  constexpr inline operator pointer() noexcept
  { return std::bit_cast<pointer>(_addr); }
  
  /***/  
  constexpr inline operator const_pointer() const noexcept
  { return std::bit_cast<const_pointer>(_addr); }

  /***/
  constexpr inline void   set_address( pointer ptr, base_t flags = 0 ) noexcept
  {
    _addr  = std::bit_cast<base_t>(ptr);
    _flags = flags;
  }

  /***/
  constexpr inline base_t flags() const noexcept 
  { return _flags; }

  /***/
  constexpr inline bool   test_flag( address_flags flag ) const noexcept 
  { return (_flags & (base_t)flag); }

  /***/
  constexpr inline void   set_flag( address_flags flag ) noexcept
  { _flags |= (base_t)flag; }

  /***/
  constexpr inline void   unset_flag( address_flags flag ) noexcept
  { _flags &= ~(base_t)flag; }
  
private:
  base_t   _addr  : conditional<(sizeof(pointer)==8), 48, 32>::value;
  base_t   _flags : conditional<(sizeof(pointer)==8), 16, 32>::value;
};

} // namespace LIB_VERSION 

}

#endif // CORE_MEMORY_ADDRESS_H
