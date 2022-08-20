#include <iostream>
#include <chrono>
#include <thread>

#include "ring_buffer.h"
#include "core/utils.h"


/**
 * The following program make use of multi-queue template.
 * 
 * The application spown 8 different thread, making each one of them to populate a specified queue.
 * Each thread will push back 1 Million items, so at the end we will have 8 Million items in the multi-queue.
 * The time required for 8 Million of push will be shown. 
 * Since we used to pre-allocate all nodes exectution will be super fast.
 * 
 * At this stage the main thread will call 8 Million times the pop() function in order to remove all items from the queue. 
 */
int main( int argc, const char* argv[] )
{ 
  (void)argc;
  (void)argv;

  const uint32_t single_thread_repeat = 1000000;
  const uint32_t nthreads = 8;

  lock_free::ring_buffer<u_int64_t,uint32_t,8000000>  rbuffer;
  
  std::thread* prod_X[nthreads];

  ////////////////////////
  // Read START TIME
  auto tp_start_ms = core::utils::now<std::chrono::milliseconds>();

  for ( uint32_t tid = 0; tid < nthreads; ++tid )
  {
    prod_X[tid] = new std::thread( [&](uint32_t th_num ){

      auto tp_start_ms = core::utils::now<std::chrono::milliseconds>();

      uint32_t conflicts = 0;
      uint32_t counter = 0;
      while (counter++ < single_thread_repeat)
      {
        auto tp_now = core::utils::now<std::chrono::nanoseconds>();
        if ( rbuffer.push(tp_now) == false)
          ++conflicts;
      }

      auto tp_end_ms = core::utils::now<std::chrono::milliseconds>();
      std::cout << "duration P_TH[" << th_num << "] " << double(tp_end_ms-tp_start_ms)/1000 << std::endl;
      std::cout << "duration P_TH[" << th_num << "] conflicts = " << conflicts << std::endl;

    }, tid );
  }
  
  for ( uint32_t tid = 0; tid < nthreads; ++tid )
  {
    prod_X[tid]->join();
  }

  ////////////////////////
  // Read END TIME
  auto tp_end_ms = core::utils::now<std::chrono::milliseconds>();
  std::cout << "duration: " << double(tp_end_ms-tp_start_ms)/1000 << std::endl;


  //////////////////////////////////////////////
  //////////////////////////////////////////////
  // POP ALL ITEMS
  ////
  u_int64_t  value = 0;
  uint32_t  pop_counter = 0;

  std::cout << "ring buffer size=" << rbuffer.size() << std::endl;
  for ( uint32_t read_counter = 0; read_counter < single_thread_repeat*nthreads; ++read_counter )
  {
    if ( rbuffer.pop( value ) == false )
      std::cout << "error pop" << std::endl;

    ++pop_counter;
  }
  std::cout << "pop_counter=" << pop_counter << std::endl;
  std::cout << "ring buffer size=" << rbuffer.size() << std::endl;

  return 0;
}