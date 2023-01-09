cmake_minimum_required(VERSION 3.16)
# Set the project name and language
project( examples
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
              "--enable=warning"
              "--inconclusive"
              "--force" 
              "--inline-suppr"
      )
  endif()
endif()

add_executable( abstract_factory             abstract_factory.cpp   )
add_executable( arena_allocator              arena_allocator.cpp    )
add_executable( rbuffer                      rbuffer.cpp            )
add_executable( mqueue                       mqueue.cpp             )
add_executable( mailbox                      mailbox.cpp            )
add_executable( singleton                    singleton.cpp          )

target_link_libraries( abstract_factory               ${DEFAULT_LIBRARIES}   )
target_link_libraries( arena_allocator                ${DEFAULT_LIBRARIES}   )
target_link_libraries( rbuffer                        ${DEFAULT_LIBRARIES}   )
target_link_libraries( mqueue                         ${DEFAULT_LIBRARIES}   )
target_link_libraries( mailbox                        ${DEFAULT_LIBRARIES}   )
target_link_libraries( singleton                      ${DEFAULT_LIBRARIES}   )