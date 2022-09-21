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

#ifndef CORE_FIXED_LOOKUP_TABLE_H
#define CORE_FIXED_LOOKUP_TABLE_H

#include <bit>
#include <type_traits>
#include <cstddef>
#include <cstdint>

namespace core {

inline namespace LIB_VERSION {

/**
 */
template< typename data_t, typename data_size_t, data_size_t items, data_t null_value >
struct fixed_lookup_table final
{
public:  
  using value_type      = data_t;
  using size_type       = data_size_t;
  using pointer         = data_t*;
  using const_pointer   = const data_t*;

public:
  /***/
  constexpr inline fixed_lookup_table() noexcept
    : _array(nullptr)
  {
    _array = new(std::nothrow) value_type[items];
    assert(_array!=nullptr) ;
    
    reset();
  }   

  /***/
  constexpr inline fixed_lookup_table( const fixed_lookup_table& ft ) noexcept = delete;

  /***/
  constexpr inline ~fixed_lookup_table()
  {
    delete [] _array;
    _array = nullptr;
  }

  /***/
  constexpr inline fixed_lookup_table& operator=( const fixed_lookup_table& ft ) noexcept = delete;
  /***/
  constexpr inline fixed_lookup_table& operator=( const fixed_lookup_table&& ft ) noexcept = delete;

  /***/
  constexpr inline const value_type& operator[]( size_type ndx ) const
  { 
    assert( ndx < items );
    return _array[ndx]; 
  }
  
  /***/
  constexpr inline bool add( size_type& index, const value_type& value )
  { 
    bool ret_val = false;

    for ( size_type ndx = 0; ndx < items; ++ndx )
    { 
      if ( _array[ndx] == null_value )
      {
        _array[ndx] = value;
        index       = ndx;
        ret_val     = true;
        break;
      } 
    }

    return ret_val;
  }

  /**
   * Reset to null_value items at @param index 
   */
  constexpr inline bool reset_at( const size_type& index )
  { 
    if ( index >= items )
        return false;

    _array[index] = null_value;

    return true;
  }

  /**
   * Reset to null_value all items that match with @param value 
   */
  constexpr inline bool reset_value( const value_type& value )
  { 
    bool ret_val = false;

    for ( size_type ndx = 0; ndx < items; ++ndx )
    { 
      if ( _array[ndx] == value )
      {
        _array[ndx] = null_value;
        ret_val     = true;
      } 
    }

    return ret_val;
  }

  /**
   * Reset overall items to null_value
   */
  constexpr inline void reset()
  { 
    for ( size_type ndx = 0; ndx < items; ++ndx )
    { _array[ndx] = null_value; }
  }

private:
  value_type*   _array;

};

} // namespace LIB_VERSION 

}

#endif // CORE_FIXED_LOCKUP_TABLE_H
