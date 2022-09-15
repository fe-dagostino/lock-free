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

#ifndef CORE_UTILS_H
#define CORE_UTILS_H

#include "core/types.h"
#include <cstring>
#include <vector>

using namespace std::chrono_literals;

namespace core {

inline namespace LIB_VERSION {

/**
 * @brief Class intended with generic functionality.
 */
class utils
{
public:
  /**
   * @brief Return current time accordingly with T.
   * 
   * @tparam T specify conversion for now(), it should be one of the following values:
   *         - std::chrono::nanoseconds
   *         - std::chrono::microseconds
   *         - std::chrono::milliseconds
   *         - std::chrono::seconds
   *         - std::chrono::minutes
   *         - std::chrono::hours
   *         - std::chrono::days
   *         - std::chrono::weeks
   *         - std::chrono::years
   *         - std::chrono::months
   * @return auto 
   */
  template<typename T>
  requires    std::is_same_v<T, std::chrono::nanoseconds>  || std::is_same_v<T, std::chrono::microseconds> 
           || std::is_same_v<T, std::chrono::milliseconds> || std::is_same_v<T, std::chrono::seconds> 
           || std::is_same_v<T, std::chrono::minutes>      || std::is_same_v<T, std::chrono::hours> 
           || std::is_same_v<T, std::chrono::days>         || std::is_same_v<T, std::chrono::weeks> 
           || std::is_same_v<T, std::chrono::years>        || std::is_same_v<T, std::chrono::months> 
  static constexpr auto now() noexcept
  { return std::chrono::time_point_cast<T>(std::chrono::steady_clock::now()).time_since_epoch().count(); }

  /**
   * @brief Waiting std::format to be fully supported.
   */
  template<typename ... Args>
  static constexpr std::string format( const char* format, Args ... args ) noexcept
  {
    int buffer_size = std::snprintf( nullptr, 0, format, args ... );
    if ( buffer_size <= 0 )
      return std::string();

    std::vector<char> buffer(buffer_size + 1);        
    std::snprintf(&buffer[0], buffer.size(), format, args ... );
    
    return std::string( buffer.begin(), buffer.end()-1 );
  }

  /**
   * @brief Split a source string in tokens using specified delimeters.
   * 
   * @param str           r/w buffer will be modified. 
   *                      The application is in charge to create a copy for str or not.
   * @param delimiters    list of characters used as delimeter between two token.
   * @return tokens       a vector with all tokens in the same order they were retrieved from @str
   */
  static std::vector<std::string> strtok( std::string& str, const std::string& delimiters )
  {
    std::vector<std::string> _tokens;
    strtok( str, delimiters, _tokens );
    return _tokens;
  }

  /**
   * @brief Split a source string in tokens using specified delimeters.
   * 
   * @param str           r/w buffer will be modified. 
   *                      The application is in charge to create a copy for str or not.
   * @param delimiters    list of characters used as delimeter between two token.
   * @param tokens        output vector used to store all tokens in the same order they were retrieved from @str
   * @return true         @tokens can be empty if no token were found.
   * @return false        str or delimeters were empty.
   */
  static bool                     strtok( std::string& str, const std::string& delimiters, std::vector<std::string>& tokens )
  {
    if ( str.empty()  || delimiters.empty() )
      return false;

    /* cool but only works with GCC and not with CLang 14.0 so we keep a retro' implementation for now in
       order to support the same code with different copilers. When ranges will be fully implemented also 
       in CLang this code will be used a generic implementation.

    std::string_view words(str);
    std::string_view delim(delimiters);
    for (const auto word : std::views::split(words, delim)) 
    { tokens.emplace_back( std::string_view(word.begin(), word.end()) ); }
    */
    char *cursor = str.data();
    char *token  = std::strtok(cursor, delimiters.c_str());
    while (token) 
    {
      tokens.emplace_back( token );
      token = std::strtok(nullptr, delimiters.c_str());
    }

    return true;
  }


};


} // namespace LIB_VERSION 

}

#endif // CORE_UTILS_H
