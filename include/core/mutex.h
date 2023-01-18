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

#ifndef CORE_MUTEX_H
#define CORE_MUTEX_H

#include <atomic>

namespace core {

inline namespace LF_LIB_VERSION {

/**
 * @brief A mutex with a spin lock implementation that leverage on atomic<bool>.
 *        This is an alterantive to std::mutex which is based on native mutexes,
 *        but in high contended contexts is much slower than a spin-lock.
 * 
 *        Unfortunately futex are not part of the standard, so right now this
 *        seems to be a good compromise between performances and portability. 
 */
class mutex final {
public:
  constexpr inline mutex() noexcept
   : _lock(false)
  {}

  /***/
  inline void lock() noexcept
  {
    do{
    } while(std::atomic_exchange_explicit(&_lock, true, std::memory_order_acquire));
  }

  /***/
  inline bool try_lock() noexcept
  { return !std::atomic_exchange_explicit(&_lock, true, std::memory_order_acquire); }

  /***/
  inline void unlock() noexcept
  { std::atomic_store_explicit(&_lock, false, std::memory_order_release); }

private:
  std::atomic<bool> _lock;
};


} // namespace LF_LIB_VERSION 

}

#endif // CORE_MUTEX_H
