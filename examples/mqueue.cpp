#include <iostream>
#include <chrono>
#include <thread>

#include "multi_queue.h"

using namespace std::chrono_literals;

class utils
{
public:
  /**
   * @brief 
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

};

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

  const uint32_t single_thread_repeat = 5000000;
  const uint32_t nthreads = 8;

  lock_free::multi_queue<u_int64_t,uint32_t, nthreads,1024,10000000,0>  mqueue;
  
  std::thread* prod_X[nthreads];

  ////////////////////////
  // Read START TIME
  auto tp_start_ms = utils::now<std::chrono::milliseconds>();

  for ( uint32_t tid = 0; tid < nthreads; ++tid )
  {
    prod_X[tid] = new std::thread( [&](uint32_t th_num ){

      auto tp_start_ms = utils::now<std::chrono::milliseconds>();

      uint32_t conflicts = 0;
      uint32_t counter = 0;
      while (counter++ < single_thread_repeat)
      {
        auto tp_now = utils::now<std::chrono::nanoseconds>();

        // enqueue in a well defined queue_id
        //if ( mqueue.push( th_num, tp_now) != core::result_t::eSuccess)
        
        // distributing load based on thread::id
        if ( mqueue.push( tp_now) != core::result_t::eSuccess)
          ++conflicts;
      }

      auto tp_end_ms = utils::now<std::chrono::milliseconds>();
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
  auto tp_end_ms = utils::now<std::chrono::milliseconds>();
  std::cout << "duration: " << double(tp_end_ms-tp_start_ms)/1000 << std::endl;


  //////////////////////////////////////////////
  //////////////////////////////////////////////
  // POP ALL ITEMS
  ////
  uint64_t  value = 0;
  uint32_t  pop_counter = 0;

  std::cout << "queue size=" << mqueue.size() << std::endl;
  for ( uint32_t read_counter = 0; read_counter < single_thread_repeat*nthreads; ++read_counter )
  {
    if ( mqueue.pop( value ) != core::result_t::eSuccess )
    {
      //std::cout << "error pop" << std::endl;
    }

    ++pop_counter;
  }
  std::cout << "pop_counter=" << pop_counter << std::endl;
  std::cout << "queue size=" << mqueue.size() << std::endl;

  return 0;
}