cmake_minimum_required(VERSION 3.16)
# Set the project name and language
project( benchmarks
         LANGUAGES CXX C
)

# Add the temporary output directories to the library path to make sure that
# parent files can be found, even if it is not installed system-wide yet.
LINK_DIRECTORIES( ${LIB_BINARY_DIR} )

include(FetchContent)

# Download google benchmark
#FetchContent_Declare(
#    benchmark
#    GIT_REPOSITORY https://github.com/google/benchmark.git
#    GIT_TAG v1.7.0
#    GIT_SHALLOW TRUE)

#FetchContent_MakeAvailable(benchmark)
#add_subdirectory(benchmark)

# Download nanobench
FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.3.7
    GIT_SHALLOW TRUE)


FetchContent_MakeAvailable(nanobench)

set(  DEFAULT_LIBRARIES
      nanobench
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

add_executable( bm_arena_allocator                       bm_arena_allocator.cpp           )
add_executable( bm_mt_arena_allocator                    bm_mt_arena_allocator.cpp        )
add_executable( bm_mt_arena_allocator_stress             bm_mt_arena_allocator_stress.cpp )
add_executable( bm_mt_queue                              bm_mt_queue.cpp                  )
add_executable( bm_mt_stack                              bm_mt_stack.cpp                  )

target_link_libraries( bm_arena_allocator                ${DEFAULT_LIBRARIES}         )
target_link_libraries( bm_mt_arena_allocator             ${DEFAULT_LIBRARIES}         )
target_link_libraries( bm_mt_arena_allocator_stress      ${DEFAULT_LIBRARIES}         )
target_link_libraries( bm_mt_queue                       ${DEFAULT_LIBRARIES}         )
target_link_libraries( bm_mt_stack                       ${DEFAULT_LIBRARIES}         )