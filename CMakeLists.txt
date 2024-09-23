cmake_minimum_required(VERSION 3.16)

set(lf_libname "lock-free")

# Set the project name and language
project( ${lf_libname}
         VERSION 0.1.0
         DESCRIPTION "lock-free data structures."
         LANGUAGES CXX C
)

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h)

target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include )

# uncomment to set a default CXX standard for the external tools like clang-tidy and cppcheck
# and the targets that do not specify a standard.
# If not set, the latest supported standard for your compiler is used
# You can later set fine-grained standards for each target using `target_compile_features`
# Note: linking together projects compiled with different C++ standards may work, but
# it is not recommended because of possible issues with ABI
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example
# when compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS ON)

if(GENERATOR_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
  # Make sure that all supported configuration types have their
  # associated conan packages available. You can reduce this
  # list to only the configuration types you use, but only if one
  # is not forced-set on the command line for VS
  set(CMAKE_CONFIGURATION_TYPES
      Debug
      Release
      RelWithDebInfo
      MinSizeRel)
endif()


if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
  set(  LF_LIB_VERSION_SO        -${CMAKE_PROJECT_VERSION}-debug          )
  set(  LF_LIB_VERSION_A         -${CMAKE_PROJECT_VERSION}-static-debug   )
else()
  set(  LF_LIB_VERSION_SO        -${CMAKE_PROJECT_VERSION}                )
  set(  LF_LIB_VERSION_A         -${CMAKE_PROJECT_VERSION}-static         )
endif()

set(  LF_LIB_BINARY_DIR 
      ${CMAKE_CURRENT_BINARY_DIR}
   )

if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
  add_definitions(
                  -ggdb3
  )
endif()

if(MSVC)
  # warning level 4 and all warnings as errors
  add_compile_options(-D_CRT_SECURE_NO_WARNINGS /W4 /WX)
else()
  # lots of warning
  add_compile_options( -march=native -mtune=native -Wall -Wextra -pedantic )
  add_link_options( -lm -lpthread -latomic )
  
  if( CMAKE_BUILD_TYPE STREQUAL "Release" )
  add_compile_options( -O3 )
  endif()
endif()

option(LF_BUILD_BENCHMARKS "Enable/Disable benchmarks build"  OFF)
option(LF_BUILD_EXAMPLES   "Enable/Disable examples build"    OFF)
option(LF_BUILD_TESTS      "Enable/Disable tests build"       OFF)

file( GLOB_RECURSE 
      LIB_INCLUDE
      ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    )
    
file( GLOB_RECURSE 
      LIB_SRC
      ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )

add_library ( ${lf_libname}  INTERFACE )
set_target_properties(${lf_libname} PROPERTIES PUBLIC_HEADER "${LIB_INCLUDE}" )

target_include_directories( ${lf_libname}
                            INTERFACE
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install( TARGETS ${lf_libname}  ARCHIVE DESTINATION lib )
install( FILES   ${LIB_INCLUDE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} ) 

if ( LF_BUILD_BENCHMARKS )
  add_subdirectory(benchmarks)
endif()

if ( LF_BUILD_EXAMPLES )
  add_subdirectory(examples)
endif()

if ( LF_BUILD_TESTS )
  add_subdirectory(tests)
endif()
