#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <iostream>

#include "core/arena_allocator.h"
#include "arena_allocator.h"

/**
 * @brief data items used to perform operations in both 
 *        constructor and destructor, avoiding optimizatios.
 */
struct data_item_t {
  
  constexpr data_item_t() noexcept
  {
    for ( uint32_t i = 0; i < 12; i++ )
      data[i]=i;
  }

  constexpr data_item_t( uint32_t value ) noexcept
  {
    for ( uint32_t i = 0; i < 12; i++ )
      data[i]=value;
  }

  constexpr ~data_item_t() noexcept
  {
    for ( uint32_t i = 0; i < 12; i++ )
      data[i]=0;
  }

  uint32_t  data[12];
};

/**
 * The following program make use of arena_allocator template.
 * 
 */
int main( int argc, const char* argv[] )
{ 
  (void)argc;
  (void)argv;

  const uint32_t c_pre_items = 1000000;
  
  lock_free::arena_allocator<data_item_t,uint32_t,c_pre_items,c_pre_items, 0, 0 >   arena_lock_free;
  core::arena_allocator<data_item_t,uint32_t,c_pre_items,c_pre_items, 0, 0 >        arena_mutex;

  data_item_t* arrItems[c_pre_items];

  ankerl::nanobench::Bench bench;

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(50)
  .batch(c_pre_items)
  .run("Using new and delete", [&] {

        for ( uint32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = new data_item_t(i);
        for ( uint32_t i = 0; i < c_pre_items; ++i )
          delete arrItems[i];

    });

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(50)
  .batch(c_pre_items)
  .run("Using Lock-Free arena_allocator", [&] {

        for ( uint32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = arena_lock_free.allocate(i);
        for ( uint32_t i = 0; i < c_pre_items; ++i )
          (void)arena_lock_free.deallocate(arrItems[i]);
        
    });

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(50)
  .batch(c_pre_items)
  .run("Using Lock-Free arena_allocator unsafe", [&] {

      for ( uint32_t repeat = 0; repeat < 1; ++repeat )
      {
        for ( uint32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = arena_lock_free.unsafe_allocate(i);
        for ( uint32_t i = 0; i < c_pre_items; ++i )
          (void)arena_lock_free.unsafe_deallocate(arrItems[i]);
      }
        
    });

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(50)
  .batch(c_pre_items)
  .run("Using Core arena_allocator", [&] {

        for ( uint32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = arena_mutex.allocate(i);
        for ( uint32_t i = 0; i < c_pre_items; ++i )
          (void)arena_mutex.deallocate(arrItems[i]);
        
    });

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(50)
  .batch(c_pre_items)
  .run("Using Core arena_allocator unsafe", [&] {

      for ( uint32_t repeat = 0; repeat < 1; ++repeat )
      {
        for ( uint32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = arena_mutex.unsafe_allocate(i);
        for ( uint32_t i = 0; i < c_pre_items; ++i )
          (void)arena_mutex.unsafe_deallocate(arrItems[i]);
      }
        
    });

  std::ofstream _output_csv ("arena_allocator.csv");
  ankerl::nanobench::render( ankerl::nanobench::templates::csv() , bench, _output_csv  );

  std::ofstream _output_json("arena_allocator.json");
  ankerl::nanobench::render( ankerl::nanobench::templates::json(), bench, _output_json );
 
  return 0;
}