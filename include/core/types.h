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

#include <bit>
#include <type_traits>
#include <cstddef>
#include <cstdint>

namespace core {

inline namespace LIB_VERSION {

/**
 * @brief Similar to conditional_t, the same pattern have been applied to 
 *        values.
 */
template<typename value_type, bool B, value_type T, value_type F>
struct conditional                         { enum : value_type { value = T }; };
/* specialization for 'false' */
template<typename value_type, value_type T, value_type F>
struct conditional<value_type,false, T, F> { enum : value_type { value = F }; };


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
struct node_t_prev {
  static constexpr bool has_prev = true;

  node_t_prev()
    : _prev(nullptr)
  {}
  
  using pointer = node_t<value_type,add_prev,add_next,use_atomic>*;

  typedef std::conditional_t<use_atomic, std::atomic<pointer>, pointer>   node_type;

  node_type   _prev;
};

/**
 * @brief specialization for add_prev == false.
 */
template <typename value_type,bool add_next,bool use_atomic>
struct node_t_prev<value_type,false,add_next,use_atomic> {
  static constexpr bool has_prev = false;
};

/**
 * @brief template structure that will be specialized with a data memeber by default.
 */
template<typename value_type,bool add_prev,bool add_next, bool use_atomic>
struct node_t_next {
  static constexpr bool has_next = true;

  node_t_next()
    : _next(nullptr)
  {}

  using pointer = node_t<value_type,add_prev,add_next,use_atomic>*;

  typedef std::conditional_t<use_atomic, std::atomic<pointer>, pointer>   node_type;

  node_type   _next;
};

/**
 * @brief specialization for add_next == false.
 */
template <typename value_type,bool add_prev,bool use_atomic>
struct node_t_next<value_type,add_prev,false,use_atomic> {
  static constexpr bool has_next = false;    
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
struct node_t : node_t_prev<value_type, add_prev, add_next, use_atomic>, 
                node_t_next<value_type, add_prev, add_next, use_atomic> 
{
  /**
   * @brief Default constructor.
   */
  constexpr inline node_t() noexcept
  { }

  /**
   * @brief Constructor thaht allow to specify rvalue for the node.
   */
  constexpr inline node_t( const value_type& value ) noexcept
    : _data(value)
  { }

  /**
   * @brief Constructor that allow to specify value for the node.
   */
  constexpr inline node_t( value_type&& value ) noexcept
    : _data( std::move(value) )
  { }

  /**
   * @brief Destructor.
   *        Note: deallocation is performed at queue_t level in order to avoid
   *              stack overflow when deallocating millions of items.
   */
  constexpr inline ~node_t() noexcept
  { }

  value_type    _data;
};




} // namespace LIB_VERSION 

}

#endif // CORE_TYPES_H
