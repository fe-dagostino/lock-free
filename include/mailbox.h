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

#ifndef CORE_MAILBOX_H
#define CORE_MAILBOX_H

#include "queue.h"
#include "core/event.h"

namespace lock_free {

inline namespace LIB_VERSION {

/**
 * @brief A mailbox implementation.
 */
template<typename data_t, core::ds_impl_t imp_type, uint32_t size_limit = 0 >
class mailbox : protected lock_free::queue< data_t, uint32_t, imp_type, 1024, 1024, size_limit >
{
public:
  using queue_type = lock_free::queue< data_t, uint32_t, imp_type, 1024, 1024, size_limit >;
  using value_type = data_t;
  
  /***/
  constexpr inline mailbox( const std::string& name ) noexcept
    : _name(name)
  {}

  /***/
  using queue_type::size;
  /***/
  using queue_type::empty;

  /***/
  constexpr inline const std::string&  name() const
  { return _name; }

  /***/
  constexpr inline core::result_t      read( value_type& data, uint32_t timeout )
  {
    if ( _event.wait( timeout ) == core::result_t::eTimeout )
      return core::result_t::eTimeout;

    return queue_type::pop( data );
  }

  /***/
  template<typename value_type>
  constexpr inline core::result_t      write( value_type&& data ) noexcept
  {
    core::result_t result = queue_type::push( std::move(data) );
    if ( result == core::result_t::eSuccess )
    { _event.signal(); }

    return result;
  }

private:
  const std::string   _name;
  core::event         _event;
};


} // namespace LIB_VERSION 

}

#endif // CORE_MAILBOX_H
