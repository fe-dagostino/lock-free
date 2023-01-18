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

#ifndef CORE_SINGLETON_H
#define CORE_SINGLETON_H

#include "types.h"

namespace core {

inline namespace LF_LIB_VERSION {

/**
 * @brief A singleton base class intended to be used in different contexts due to 
 *        the possibility to call constructors with parameters as well as to minimize
 *        all conditional check at runtime due to initialization leveraging N2660
 *        ( https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2660.htm ).
 * 
 * @tparam derived_t derived class.
 */
template<typename derived_t>
  requires std::is_class_v<derived_t>
class singleton_t
{
public:  
  /**
   * @brief This method must be called once when the program start its execution.
   *        initialize() allow to call a derived_t contructor with an arbitrary number of arguments.
   *        Once the instance is ready and static memory have been initialized, then the on_initialize() method
   *        will be invoked in order to allow other operations that maybe required during initialization.
   *        derived_t's constructors can be kept protected using just using friend class declaration.
   * 
   * @return true   if the singleton instance have been instantiated and initializated, since this moment each
   *                access to the instance should be done using get_instance() method.
   * @return false  if initialize() have been already called, or if there is an no available memory to create
   *                the singleson instance.
   */
  template<typename... args_t>
  static bool   initialize( args_t&&... args ) noexcept
  {
    if ( m_instance )
      return false;

    // Used 'new' directly to avoid std::make_unique limitations accessing protected or private constructors.
    m_instance = std::unique_ptr<derived_t>( new(std::nothrow) derived_t(std::forward<args_t&&>(args)...) );
    if ( m_instance == nullptr )
      return false;

    m_instance->on_initialize();

    return true;
  }

  /**
   * @brief singleton is considered valid in case it have been correctly initialized.
   * 
   * @return true  singleton have been initialized
   * @return false singleton have been no initialized
   */
  static constexpr bool   is_valid() noexcept
  { return (m_instance != nullptr); }

  /**
   * @brief Get the instance object
   * 
   * @return derived_t* 
   */
  static derived_t* get_instance() noexcept
  { return m_instance.get(); }

  /**
   * @brief This method is intended to be called at the end of the program exectution
   *        in order to release resources or any other operation required before to exit
   *        the process.
   */
  void finalize()
  { static_cast<derived_t*>(this)->on_finalize(); }

protected:  
  /**
   * @brief invoked by initialize(), this method if defined in derived_t will be overrided, 
   *        so derived_t::on_initialize() will be invoked. If not defined in derived_t then
   *        this implementation will be the defalt and will do nothing.
   */
  constexpr void on_initialize() noexcept { }
  /**
   * @brief invoked by finalize(), this method if defined in derived_t will be overrided, 
   *        so derived_t::on_finalize() will be invoked. If not defined in derived_t then
   *        this implementation will be the defalt and will do nothing.
   */
  constexpr void on_finalize()   noexcept { }

private:
  inline static std::unique_ptr<derived_t>   m_instance = nullptr;
};

} // namespace LF_LIB_VERSION 

}

#endif // CORE_SINGLETON_H