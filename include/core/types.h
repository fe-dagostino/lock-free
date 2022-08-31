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

#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <algorithm>

#include <mutex>
#include "core/mutex.h"


#include <thread>
#include <atomic>
#include <condition_variable>

using namespace std::chrono_literals;

namespace core {

inline namespace LIB_VERSION {

enum class result_t {
    eFailure         =    0,
    eSuccess         =    1,

    eEmpty           =    2,
    
    eNullPointer     =  100,
    eDoubleFree      =  101,
    
    eNotImplemented  =  204,

    eTimeout         =  408,
    eSignaled        =  409
};

enum class ds_impl_t {
  raw,
  mutex,
  spinlock,
  lockfree
};

/*********************/
template<std::size_t size>
struct tstring_t {
    /***/
    constexpr tstring_t(const char (&str)[size]) 
    { std::copy_n(str, size, _in_string); }

    /**
     * Return string length.
     */
    constexpr std::size_t      length()  const 
    { return size-1; }
    /**
     * Return a pointer to the null terminated string.
     */
    constexpr const char* c_str() const
    { return &_in_string[0]; }

    char _in_string[size];
};

/*********************/
template<tstring_t str>
struct plug_name
{ 
  constexpr static const bool             has_name = (str.length()>1)?true:false;
  constexpr static const std::string_view name     = str.c_str();
};

/**
 * @brief Define concept for plug_name interface requirements.
 */
template <typename P>
concept plug_name_interface = requires( ) 
{
  { P::has_name };
  { P::name     };
};

/*********************/

/**
 * @brief Similar to conditional_t, the same pattern have been applied to 
 *        values.
 */
template<typename value_type, bool B, value_type T, value_type F>
struct conditional                         { enum : value_type { value = T }; };
/* specialization for 'false' */
template<typename value_type, value_type T, value_type F>
struct conditional<value_type,false, T, F> { enum : value_type { value = F }; };

/*********************/

/**
 * @brief std::is_base_of extended for multiple types
 */
template<typename base_t, typename... others_t>
struct are_base_of
{ constexpr static const bool value = std::conjunction_v<std::is_base_of<base_t,others_t>... >; };

/*********************/

/**
 * @brief Define concept checking that all @tparamother_t types are derived from @base_t.
 */
template<typename base_t, typename... others_t>
concept derived_types = requires
{
  requires are_base_of<base_t,others_t...>::value;
};

/*********************/

/**
 * @brief Define concept for mutex interface in order.
 */
template <typename M>
concept mutex_interface = requires( M& m ) 
{
  { m.lock()     };
  { m.unlock()   };
  { m.try_lock() } -> std::same_as<bool>;
};

/*********************/

/**
 * @brief plug an optional mutex in your derived class.
 * 
 * @tparam add_mutex whether the mutex should be present or not
 * @tparam spinlock  true will use a spinklock implementation, false will use std::mutex
 */
template<bool add_mutex, typename mutex_t = core::mutex>
requires mutex_interface<mutex_t>
struct plug_mutex
{ 
  constexpr static const bool has_mutex = true;
  
  using mutex_type = mutex_t;

  constexpr inline void lock() const noexcept
  {  _mtx.lock(); }

  constexpr inline void unlock() const noexcept
  {  _mtx.unlock(); }

  constexpr inline bool try_lock() const noexcept
  {  return _mtx.try_lock(); }

  mutable mutex_type    _mtx;
};

template<typename mutex_t>
struct plug_mutex<false,mutex_t> { 
  constexpr static const bool has_mutex = false;
};

/**
 * @brief forward declaration for node_t, intended for generic usage in queue stack, double linked list
 *        as well as in lock-free data structures.
 * 
 * @tparam value_type    there are not specific restriction, can be be anything form the appliation level.
 * @tparam add_prev      if true, node_t will have a data members _prev defined as a pointer to node_t
 *                       has_prev return true or false accordingly with this value.
 * @tparam add_next      if true, node_t will have a data members _nexr defined as a pointer to node_t
 *                       has_next return true or false accordingly with this value.
 * @tparam use_atomic    specify if _prev and _next should be atomic<node_t*> or simple node_t*.
 */
template<typename value_type,bool add_prev,bool add_next, bool use_atomic>
struct node_t;

/**
 * @brief template structure that will be specialized with a data memeber by default.
 */
template<typename value_type,bool add_prev,bool add_next, bool use_atomic>
struct plug_prev {
  constexpr static bool has_prev = true;

  constexpr inline plug_prev()
    : _prev(nullptr)
  {}
  
  using pointer   = node_t<value_type,add_prev,add_next,use_atomic>*;
  using node_type = std::conditional_t<use_atomic, std::atomic<pointer>, pointer>;

  node_type   _prev;
};

/**
 * @brief specialization for add_prev == false.
 */
template <typename value_type,bool add_next,bool use_atomic>
struct plug_prev<value_type,false,add_next,use_atomic> {
  constexpr static bool has_prev = false;
};

/**
 * @brief template structure that will be specialized with a data memeber by default.
 */
template<typename value_type,bool add_prev,bool add_next, bool use_atomic>
struct plug_next {
  constexpr static bool has_next = true;

  constexpr inline plug_next()
    : _next(nullptr)
  {}

  using pointer   = node_t<value_type,add_prev,add_next,use_atomic>*;
  using node_type = std::conditional_t<use_atomic, std::atomic<pointer>, pointer>;

  node_type   _next;
};

/**
 * @brief specialization for add_next == false.
 */
template <typename value_type,bool add_prev,bool use_atomic>
struct plug_next<value_type,add_prev,false,use_atomic> {
  constexpr static bool has_next = false;    
};

/**
 * @brief check forward declaration above.
 *        node_t extends both node_t_prev<> and node_t_next<> 
 *        that accordingly with template parameters can be empty definitions
 *        or a pointer to prev and next.
 * 
 *   Note: inner forward declaration for templates is not supported, so 
 *         externalize node_t is workaround.
 */
template<typename value_type, bool add_prev,bool add_next, bool use_atomic>
struct node_t : plug_prev<value_type, add_prev, add_next, use_atomic>, 
                plug_next<value_type, add_prev, add_next, use_atomic> 
{
  using node_prev_type = plug_prev<value_type,add_prev,add_next,use_atomic>;
  using node_next_type = plug_next<value_type,add_prev,add_next,use_atomic>;
  using pointer        = node_t<value_type,add_prev,add_next,use_atomic>*;

  /**
   * @brief Default constructor.
   */
  constexpr inline node_t() noexcept
  { }

  /**
   * @brief Copy Constructor.
   */
  constexpr inline explicit node_t( const value_type& value ) noexcept
    : _data(value)
  { }

  /**
   * @brief Constructor with move semantic.
   */
  constexpr inline explicit node_t( value_type&& value ) noexcept
    : _data( std::move(value) )
  { }

  /**
   * @brief Destructor.
   */
  constexpr inline ~node_t() noexcept
  { }

  /***/
  constexpr inline node_t& operator=( const node_t& node ) noexcept
  {
    if constexpr (node_prev_type::has_prev==true)
    { node_prev_type::_prev = node._prev; }

    if constexpr (node_next_type::has_next==true)
    { node_next_type::_next = node._next; }

    _data = node._data;

    return *this;
  }

  /***/
  constexpr inline node_t& operator=( node_t&& node ) noexcept
  {
    if constexpr (node_prev_type::has_prev==true)
    { node_prev_type::_prev = std::move(node._prev); }

    if constexpr (node_next_type::has_next==true)
    { node_next_type::_next = std::move(node._next); }

    _data = std::move(node._data);

    return *this;
  }

  /***/
  constexpr inline pointer get_prev() const
  { 
    if constexpr ( node_prev_type::has_prev == true )  // also add_next can be used here.
    {
      if constexpr (use_atomic == true )
      {  return node_prev_type::_prev.load(); }

      if constexpr (use_atomic == false )
      {  return node_prev_type::_prev; }
    }
   
    return nullptr;
  }

  /***/
  constexpr inline pointer get_next() const
  { 
    if constexpr ( node_next_type::has_next == true )  // also add_next can be used here.
    {
      if constexpr (use_atomic == true )
      {  return node_next_type::_next.load(); }

      if constexpr (use_atomic == false )
      {  return node_next_type::_next; }
    }
   
    return nullptr;
  }

  value_type    _data;
};

/**
 * @brief Since only gcc updated std::lock_guard to constexpr this 
 *        class will be used as replacement for std::lock_guard in 
 *        order to guarantee compatibility with other compilers.
 * 
 * @tparam mutex_t   mutex class this mutex_interface requirements.
 */
template<typename mutex_t>
requires mutex_interface<mutex_t> 
class lock_guard final
{
public:
  /***/
  constexpr inline explicit lock_guard( mutex_t& mtx ) noexcept
    : _mtx(mtx)
  { _mtx.lock(); }
  /***/
  constexpr inline ~lock_guard( ) noexcept
  { _mtx.unlock(); }

private:
  mutex_t& _mtx;
};


} // namespace LIB_VERSION 

}

#endif // CORE_TYPES_H
