cmake_minimum_required(VERSION 3.16)
# Set the project name and language
project( examples
         LANGUAGES CXX C
)

# Add the temporary output directories to the library path to make sure that
# libcsv can be found, even if it is not installed system-wide yet.
LINK_DIRECTORIES( ${LIB_BINARY_DIR} )

set(  DEFAULT_LIBRARIES
      
)

option(LF_ENABLE_CPPCHECK    "Enable/Disable cppcheck"    OFF)

if ( LF_ENABLE_CPPCHECK )
  set(CMAKE_CXX_CPPCHECK "cppcheck")
endif()

add_executable( rbuffer                      rbuffer.cpp            )
add_executable( mqueue                       mqueue.cpp             )


target_link_libraries( rbuffer                        ${DEFAULT_LIBRARIES}   )
target_link_libraries( mqueue                         ${DEFAULT_LIBRARIES}   )
