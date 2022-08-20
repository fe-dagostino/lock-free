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

#ifndef CORE_MEM_UNIQUE_PTR_H
#define CORE_MEM_UNIQUE_PTR_H

#include <memory>

#include "memory_address.h"


namespace core {

inline namespace LIB_VERSION {


template<class T, class U>
concept Derived = std::is_base_of<U, T>::value;

template<class T>
concept Data_t = (    std::is_object_v<T>
                   && !std::is_array_v<T> 
                   && !std::is_pointer_v<T> );

template <Data_t data_t>
class mem_unique_ptr
{
  using value_type      = data_t; 
  using reference       = value_type&;
  using const_reference = const value_type&;
  using pointer         = value_type*;
  using const_pointer   = const value_type*;

  using mem_address     = memory_address<value_type,size_t>;

public:
  /***/
  constexpr inline mem_unique_ptr( ) noexcept
    : _ptr( )
  { }
  /***/
  constexpr inline mem_unique_ptr( std::nullptr_t ) noexcept
    : _ptr( )
  { }

  /**
   * @brief explicity disable copy constructor.
   */
  constexpr inline mem_unique_ptr( const mem_unique_ptr<value_type>& ptr ) noexcept = delete;

  /***/
  template<Derived<value_type> T>
  constexpr inline mem_unique_ptr( T* ptr, bool autodelete = true ) noexcept
    : _ptr( ptr, (autodelete)?(uint32_t)(mem_address::address_flags::DESTROY):(uint32_t)0, 0 )
  { }

  /***/
  template<Derived<value_type> T>
  constexpr inline mem_unique_ptr( std::unique_ptr<T>&& ptr, bool autodelete = true ) noexcept
    : _ptr( ptr.release(), (autodelete)?(uint32_t)(mem_address::address_flags::DESTROY):(uint32_t)0, 0 )
  { }

  /***/
  template<Derived<value_type> T>
  constexpr inline mem_unique_ptr( mem_unique_ptr<T>&& ptr ) noexcept
    : _ptr( ptr.release(), (ptr.auto_delete())?(uint32_t)(mem_address::address_flags::DESTROY):(uint32_t)0, 0 )
  { }

  /***/
  constexpr inline ~mem_unique_ptr() noexcept
  {
    if ((auto_delete()) && ( _ptr != nullptr ))
    {
      delete (pointer)_ptr;
      _ptr.set_address( nullptr, 0 );
    }
  }

  /**
   * @brief explicity disable assignment operator.
   */
  constexpr inline mem_unique_ptr<value_type>& operator=( const mem_unique_ptr<value_type>& ptr ) noexcept = delete;

  /***/
  template<Derived<value_type> T>
  constexpr inline reference operator=( std::unique_ptr<T>&& ptr ) noexcept
  { 
    _ptr.set_address( ptr.release(), mem_address::address_flags::DESTROY );
    return *this; 
  }

  /***/
  template<Derived<value_type> T>
  constexpr inline reference operator=( mem_unique_ptr<T>&& ptr ) noexcept
  { 
    _ptr.set_address( ptr.release(), (ptr.auto_delete())?mem_address::address_flags::DESTROY:0 );
    return *this; 
  }

  /***/
  constexpr inline reference        operator*() const noexcept
  {
    static_assert(get() != pointer());
    return *get();
  }

  /***/
  constexpr inline pointer          operator->() const noexcept
  { return get(); }
  
  /***/
  constexpr inline pointer          get() const noexcept
  { return _ptr; }

  /***/
  constexpr inline bool    auto_delete() const noexcept
  { return _ptr.test_flag(mem_address::address_flags::DESTROY) ; }
  /***/
  constexpr inline void    auto_delete( bool autodelete ) noexcept
  { (autodelete)?_ptr.set_flag(mem_address::address_flags::DESTROY):_ptr.unset_flag(mem_address::address_flags::DESTROY); }

  /* Return true if the stored pointer is not null. */
  constexpr inline operator bool() const noexcept
  { return get() == pointer() ? false : true; }

  constexpr inline pointer release() noexcept
  { 
    pointer ret_ptr = _ptr;
    _ptr.set_address( nullptr, 0 );
    return ret_ptr;
  }

private:
  mem_address  _ptr;
};

template<typename data_t>
bool operator==( const mem_unique_ptr<data_t>& lhs, const mem_unique_ptr<data_t>& rhs)
{ return (lhs.get() == rhs.get()); }

template<typename data_t>
bool operator==(const mem_unique_ptr<data_t>& lhs, std::nullptr_t ) noexcept
{ return !(bool)lhs; }

template<typename data_t>
bool operator!=( const mem_unique_ptr<data_t>& lhs, const mem_unique_ptr<data_t>& rhs)
{ return (lhs.get() != rhs.get()); }

template<typename data_t>
bool operator!=(const mem_unique_ptr<data_t>& lhs, std::nullptr_t ) noexcept
{ return (bool)lhs; }

} // namespace LIB_VERSION 

}

#endif // CORE_MEM_UNIQUE_PTR_H
