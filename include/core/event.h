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

#ifndef CORE_EVENT_H
#define CORE_EVENT_H

#include "core/types.h"

namespace core {

inline namespace LIB_VERSION {

/**
 * @brief An event object based on condition variable.
 */
class event final 
{
public:
  using mutex_type = std::mutex;
  using cv_type    = std::condition_variable;

  /***/
  inline event() noexcept
  {}

  /***/
  inline ~event() noexcept
  {}

  /**
   * @brief Wait for the specified timeout or until a call to signal().
   *        Note: spuriously unlock can happen even without the timeout 
   *              or signal().
   * 
   * @param timeout                number of milliseconds to wait
   * @return result_t::eTimeout when specified time elapsed or 
   *         result_t::eSignaled in any other case.
   */
  inline result_t wait( uint32_t timeout ) noexcept
  {   
    std::unique_lock<mutex_type> u_lock(_mtx);

    std::cv_status cv_res = _cv.wait_for(u_lock, timeout*1ms); 
    if  ( cv_res == std::cv_status::timeout )
      return result_t::eTimeout;

    return result_t::eSignaled;
  }

  /**
   * @brief Wait until a call to signal().
   *        Note: spuriously unlock can happen even without a signal() call.
   */
  inline result_t wait() noexcept
  {   
    std::unique_lock<mutex_type> u_lock(_mtx);

    _cv.wait(u_lock); 

    return result_t::eSignaled;
  }

  /**
   * @brief Signal the event and unlock waiting threads.
   */
  inline void     notify() noexcept
  { _cv.notify_all(); }

private:
  mutex_type  _mtx;
  cv_type     _cv;
};


} // namespace LIB_VERSION 

}

#endif // CORE_EVENT_H
