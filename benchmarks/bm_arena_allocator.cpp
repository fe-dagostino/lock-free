#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <iostream>

#include "arena_allocator.h"

/**
 * @brief data items used to perform operations in both 
 *        constructor and destructor, avoiding optimizatios.
 */
struct data_item_t {
  
  constexpr data_item_t() noexcept
  {
    for ( u_int32_t i = 0; i < 12; i++ )
      data[i]=i;
  }

  constexpr data_item_t( u_int32_t value ) noexcept
  {
    for ( u_int32_t i = 0; i < 12; i++ )
      data[i]=value;
  }

  constexpr ~data_item_t() noexcept
  {
    for ( u_int32_t i = 0; i < 12; i++ )
      data[i]=0;
  }

  u_int32_t  data[12];
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
  
  lock_free::arena_allocator<data_item_t,u_int32_t,c_pre_items>   allocator;
  
  data_item_t* arrItems[c_pre_items];

  ankerl::nanobench::Bench bench;

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(20)
  .batch(c_pre_items)
  .run("Using new and delete", [&] {

        for ( u_int32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = new data_item_t();
        for ( u_int32_t i = 0; i < c_pre_items; ++i )
          delete arrItems[i];

    });

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(20)
  .batch(c_pre_items)
  .run("Using arena_allocator", [&] {

        for ( u_int32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = allocator.allocate(1523);
        for ( u_int32_t i = 0; i < c_pre_items; ++i )
          allocator.deallocate(arrItems[i]);
        
    });

  bench
  .warmup(10)
  .epochs(10)
  .epochIterations(20)
  .batch(c_pre_items)
  .run("Using arena_allocator unsafe", [&] {

      for ( uint32_t repeat = 0; repeat < 1; ++repeat )
      {
        for ( u_int32_t i = 0; i < c_pre_items; ++i )
          arrItems[i] = allocator.unsafe_allocate(1523);
        for ( u_int32_t i = 0; i < c_pre_items; ++i )
          allocator.unsafe_deallocate(arrItems[i]);
      }
        
    });

  std::ofstream _output_csv ("arena_allocator.csv");
  ankerl::nanobench::render( ankerl::nanobench::templates::csv() , bench, _output_csv  );

  std::ofstream _output_json("arena_allocator.json");
  ankerl::nanobench::render( ankerl::nanobench::templates::json(), bench, _output_json );
 
  return 0;
}