cmake_minimum_required(VERSION 3.16)
# Set the project name and language
project( tests
         LANGUAGES CXX C
)

# Add the temporary output directories to the library path to make sure that
# parent files can be found, even if it is not installed system-wide yet.
LINK_DIRECTORIES( ${LIB_BINARY_DIR} )


set(  DEFAULT_LIBRARIES
      atomic
)

option(LF_ENABLE_CPPCHECK    "Enable/Disable cppcheck"    OFF)

if ( LF_ENABLE_CPPCHECK )
  find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
  if (CMAKE_CXX_CPPCHECK)
      list(
          APPEND CMAKE_CXX_CPPCHECK
              "--std=c++20" 
              "--enable=all"
              "--inconclusive"
              "--force" 
              "--inline-suppr"
      )
  endif()
endif()

add_executable( unique_ptr                       unique_ptr.cpp         )


target_link_libraries( unique_ptr                ${DEFAULT_LIBRARIES}   )