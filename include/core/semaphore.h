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

#ifndef CORE_SEMAPHORE_H
#define CORE_SEMAPHORE_H

#include "core/types.h"

namespace core {

inline namespace LF_LIB_VERSION {

/**
 * @brief A semaphore implementation that leverage on mutex and condition variable.
 *        This is an alterantive to std::counting_semaphore and std::binary_semaphore
 *        since both are not fully supported by all compilers.
 * 
 *        This implementation will be deleted as soon as std semaphores will be implemnted 
 *        also in compilers such as EMScripten. 
 */
template<int max_count>
  requires (max_count > 0)
class counting_semaphore final {
public:
  using mutex_type = std::mutex;
  using cv_type    = std::condition_variable;

  /***/
  constexpr counting_semaphore() = delete;
  /***/
  constexpr inline counting_semaphore( int count = max_count ) noexcept
   : _count(count)
  {}

  /***/
  inline void release() noexcept
  {
    std::unique_lock<mutex_type> u_lock(_mutex);

    if ( _count < max_count )
    {
      ++_count;
      _cv.notify_one();
    }
  }

  /***/
  inline void acquire() noexcept
  {
    std::unique_lock<mutex_type> u_lock(_mutex);

    while ( _count == 0 )
      _cv.wait(u_lock);

    --_count;
  }

private:
  int         _count;
  mutex_type  _mutex;
  cv_type     _cv;
};

typedef counting_semaphore<1> binary_semaphore;

} // namespace LF_LIB_VERSION 

}

#endif // CORE_SEMAPHORE_H
