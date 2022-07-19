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


int main( int argc, const char* argv[] )
{ 
  (void)argc;
  (void)argv;

  const u_int32_t single_thread_repeat = 1000000;
  const u_int32_t nthreads = 1;

  lock_free::multi_queue<u_int64_t,u_int32_t,nthreads,128,2000000>  mqueue;
  //moodycamel::ConcurrentQueue<u_int64_t>                mc_queue;
  
  std::thread* prod_X[nthreads];
  std::thread* cons_X[nthreads];

  ////////////////////////
  // Read START TIME
  auto tp_start_ms = utils::now<std::chrono::milliseconds>();

  for ( u_int32_t tid = 0; tid < nthreads; ++tid )
  {
    prod_X[tid] = new std::thread( [&](u_int32_t th_num ){

      auto tp_start_ms = utils::now<std::chrono::milliseconds>();

      u_int32_t conflicts = 0;
      u_int32_t counter = 0;
      while (counter++ < single_thread_repeat)
      {
#define MC_IMP  0
#if (MC_IMP == 1)
      auto tp_now = utils::now<std::chrono::nanoseconds>();
      if ( mc_queue.enqueue(tp_now) == false )
        ++conflicts;
#else
      auto tp_now = utils::now<std::chrono::nanoseconds>();
      if ( mqueue.push(th_num, tp_now) == false)
        ++conflicts;
#endif        
        
      }

      auto tp_end_ms = utils::now<std::chrono::milliseconds>();
      std::cout << "duration P_TH[" << th_num << "] " << double(tp_end_ms-tp_start_ms)/1000 << std::endl;
      std::cout << "duration P_TH[" << th_num << "] conflicts = " << conflicts << std::endl;

    }, tid );
  }
  
  for ( u_int32_t tid = 0; tid < nthreads; ++tid )
  {
    prod_X[tid]->join();
    //cons_X[tid]->join();
  }

  ////////////////////////
  // Read END TIME
  auto tp_end_ms = utils::now<std::chrono::milliseconds>();
  std::cout << "duration: " << double(tp_end_ms-tp_start_ms)/1000 << std::endl;

  u_int64_t  value = 0;
  u_int64_t  old_value = 0;
  u_int32_t  pop_counter = 0;

  std::cout << "queue size=" << mqueue.size() << std::endl;
  for ( u_int32_t read_counter = 0; read_counter <= single_thread_repeat; ++read_counter )
  {
    if ( mqueue.pop( 0, value ) == false )
      std::cout << "error pop" << std::endl;

    if ( value < old_value )
      std::cout << "error" << std::endl;
    else{
      pop_counter++;
    }

    old_value = value;
  }
  std::cout << "pop_counter=" << pop_counter << std::endl;
  std::cout << "queue size=" << mqueue.size(0) << std::endl;

  return 0;
}