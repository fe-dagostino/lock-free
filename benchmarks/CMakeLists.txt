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

# Download nanobench
FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.3.7
    GIT_SHALLOW TRUE)


FetchContent_MakeAvailable(nanobench)

set(  DEFAULT_LIBRARIES
      nanobench
)

option(LF_ENABLE_CPPCHECK    "Enable/Disable cppcheck"    OFF)

if ( LF_ENABLE_CPPCHECK )
  set(CMAKE_CXX_CPPCHECK "cppcheck")
endif()

add_executable( bm_arena_allocator              bm_arena_allocator.cpp           )
#add_executable( bm_mt_arena_allocator           bm_mt_arena_allocator.cpp        )
#add_executable( bm_arena_allocator_resize       bm_arena_allocator_resize.cpp    )
#add_executable( bm_queue                        bm_queue.cpp                     )

target_link_libraries( bm_arena_allocator                ${DEFAULT_LIBRARIES}   )
#target_link_libraries( bm_mt_arena_allocator             ${DEFAULT_LIBRARIES}   )
#target_link_libraries( bm_arena_allocator_resize         ${DEFAULT_LIBRARIES}   )
#target_link_libraries( bm_queue                          ${DEFAULT_LIBRARIES}   )