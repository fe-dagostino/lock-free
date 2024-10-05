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

#ifndef CORE_THREAD_MAP_H
#define CORE_THREAD_MAP_H

#include "config.h"
#include "types.h"
#include <unordered_map>

namespace core {

/**
 * @brief Generating an unique ID for each thread instance.
 * 
 * @tparam data_size_t  data type to be used for numeric values.
 * @tparam base         specify if first ID start from 1 (default) or
 *                      from a a different positive number.
 */
template<typename data_size_t, data_size_t base = 1>
  requires std::is_unsigned_v<data_size_t>          
struct thread_map final
{
public:  
  using value_type      = std::thread::id;
  using size_type       = data_size_t;
  using hash_map        = std::unordered_map<value_type, size_type>;

public:
  /***/
  constexpr inline thread_map() noexcept
    : _counter(base)
  {}

  /***/
  constexpr inline size_type add( const value_type& tid = std::this_thread::get_id() ) noexcept
  {
    std::lock_guard _mtx_ctrl(_mtx);

    if ( _map.contains(tid) )
      return _map[tid];
    
    _map[tid] = ++_counter;

    return _counter;
  }

  /***/
  constexpr inline bool      del( const value_type& tid = std::this_thread::get_id() ) noexcept
  {
    std::lock_guard _mtx_ctrl(_mtx);

    if ( _map.contains(tid) == false )
      return false;
    
    _map.erase(tid);

    return true;
  }

private:
  std::mutex  _mtx;
  hash_map    _map;

  size_type   _counter;
};

}

#endif // CORE_THREAD_MAP_H
