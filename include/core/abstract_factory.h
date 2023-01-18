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

#ifndef CORE_ABSTRACT_FACTORY_H
#define CORE_ABSTRACT_FACTORY_H

#include "config.h"
#include "core/types.h"

namespace core {

inline namespace LF_LIB_VERSION {

/***
 * Original design and for this class and all credits for that rely on @Quuxplusone, get the link to 
 * the question on Code Review for that:
 * https://codereview.stackexchange.com/a/240186
 * 
 * The review for such implementation is intended to go over different limitations such as:
 *   - retrieving a name to the inner template class, originally named `LabledClass` and renamed in `concrete_factory`
 *   - avoiding instantiating classes when not necessary, in fact a data member in LabeledClass was instantiating an
 *     object for each derived class.
 *   - simply dependencies check removing all static functions and replacing it with concepts
 *   - allow to have different constructor for each object and to forward all arguments
 *   - updating default, allowing the user to choice a default type to be instantiated or to return `nullptr`
 */
template<typename base_t, typename default_t, typename... others_t>
  requires derived_types<base_t,others_t...> && 
           ( std::is_base_of_v<base_t,default_t> || std::is_same_v<default_t,std::nullptr_t> )
class abstract_factory 
{
public:
  /***/
  template<typename derived_t>
    requires plug_name_interface<derived_t>
  struct concrete_factory {
    std::string_view name = derived_t::name;

    /** Create derived class instance forwarding arguments. */
    template<typename... args_t>
    std::unique_ptr<base_t> create( args_t&&... args )
    {  return std::make_unique<derived_t>( std::forward<args_t&&>(args)... ); }
  };

  /* alias */
  using concrete_factories = std::tuple<concrete_factory<others_t>...>;

  /***/
  constexpr inline abstract_factory() noexcept
  {
    /* Check that all names appears only one time*/
    constexpr bool all_names_once = all_of( concrete_factories{}, 
                                            []( const auto& tuple_item ) -> bool { 
                                              return( count_if( concrete_factories{}, 
                                                                [&id=tuple_item.name]( const auto& item ) -> std::size_t { 
                                                                    return (item.name == id); 
                                                                } ) == 1); 
                                            }   );
    static_assert(all_names_once, "name must be unique");
  }

  template<typename... args_t>
  constexpr inline std::unique_ptr<base_t> create(const std::string_view& id, args_t&&... args ) 
  {
    std::unique_ptr<base_t> result = nullptr;

    // if concrete_factory matches with the name, use the concrete factory to create the new instance.
    std::apply( [&result, &id, &args...](auto&&... tuple_item ) {
                    (( tuple_item.name == id ? result = tuple_item.create( std::forward<args_t>(args)... ) : result ), ...);
                }, concrete_factories{}
              );

    if ( result == nullptr )
    {
      if constexpr ( std::is_same_v<std::nullptr_t,default_t> == false )
      { result = std::make_unique<default_t>( std::forward<args_t>(args)... ); }
    }

    return result;
  }
};

} // namespace LF_LIB_VERSION 

}

#endif // CORE_ABSTRACT_FACTORY_H
