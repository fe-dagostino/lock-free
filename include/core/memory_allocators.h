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

#ifndef CORE_MEMORY_ALLOCATORS_H
#define CORE_MEMORY_ALLOCATORS_H

#include <type_traits>
#include <cstddef>

#ifdef _WIN32
# define WIN32_MEAN_AND_LEAN
# include <Windows.h>
#else
# include <sys/mman.h>
#endif

#include "config.h"

namespace core {

inline namespace LF_LIB_VERSION {

/***/
template<typename data_size_t>
class default_allocator 
{
public:
  using size_type       = data_size_t;

  /***/
  static constexpr inline void* allocate( const size_type& nb_bytes ) noexcept
  { return (std::aligned_alloc( alignof(std::max_align_t), nb_bytes )); }

  /***/
  static constexpr inline void  deallocate( void* ptr, [[maybe_unused]] const size_type& nb_bytes ) noexcept
  { std::free( ptr ); }

};

/***/
template<typename data_size_t>
class virtual_allocator 
{
public:
  using size_type       = data_size_t;

  /***/
  static constexpr inline void* allocate( const size_type& nb_bytes ) noexcept
#ifdef _WIN32  
  { return VirtualAlloc(NULL, nb_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }
#else
  { return mmap(NULL, nb_bytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); }
#endif

  /***/
  static constexpr inline void  deallocate( void* ptr, [[maybe_unused]] const size_type& nb_bytes ) noexcept
#ifdef _WIN32  
  { VirtualFree(reinterpret_cast<void *>(ptr), 0, MEM_RELEASE); }
#else
  { munmap(reinterpret_cast<void *>(ptr), nb_bytes); }
#endif

};

} // namespace LF_LIB_VERSION 

}

#endif // CORE_MEMORY_ALLOCATORS_H
