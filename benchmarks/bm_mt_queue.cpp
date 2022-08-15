#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <iostream>
#include <syncstream>
#include <queue>

#include "queue.h"

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
 * @brief data items used to perform operations in both 
 *        constructor and destructor, avoiding optimizatios.
 */
struct data_item_t {
  
  constexpr data_item_t() noexcept
  {
    for ( u_int32_t i = 0; i < 14; i++ )
      data[i]=i;
  }

  constexpr data_item_t( u_int32_t value ) noexcept
  {
    for ( u_int32_t i = 0; i < 14; i++ )
      data[i]=value;
  }

  constexpr ~data_item_t() noexcept
  {
    for ( u_int32_t i = 0; i < 14; i++ )
      data[i]=0;
  }

  u_int32_t  data[14];
};

struct queue_status_t {
  queue_status_t( const double& timestamp, const uint64_t& size )
   : _timestamp(timestamp), _size(size)
  {}

  double     _timestamp;
  uint64_t   _size;
};


using lock_free_queue = lock_free::queue<uint32_t,uint32_t, core::ds_impl_t::raw, 10000, 10000, 10 >;
//using lock_free_queue = lock_free::queue<uint32_t,uint32_t, core::ds_impl_t::mutex, 10000, 10000, 10 >;
//using lock_free_queue = lock_free::queue<uint32_t,uint32_t, core::ds_impl_t::spinlock, 10000, 10000, 10 >;
//using lock_free_queue = lock_free::queue<uint32_t,uint32_t, core::ds_impl_t::lockfree, 10000, 10000, 10 >;
using status_queue    = std::queue<queue_status_t>;


static void th_main_producer( uint32_t th_num, uint32_t run_time, lock_free_queue* q )
{
  uint32_t     failures   = 0;
  uint32_t     successes  = 0;
  uint32_t     cycles     = 0;

  auto th_start_ms = utils::now<std::chrono::milliseconds>();

  //double last_mon  = th_start_ms;
  for (;;)
  {
    if ( q->push(th_num+1) == core::result_t::eFailure )
      ++failures;
    else
      ++successes;
    
    ++cycles;

    auto th_end_ms = utils::now<std::chrono::milliseconds>();
    if (double(th_end_ms-th_start_ms) >= run_time)
      break;
  }

  auto th_end_ms = utils::now<std::chrono::milliseconds>();

  std::osyncstream(std::cout) << "P TH [" <<  th_num <<  "] cycles : [" << cycles << "] - successes : [" << successes << "] - failures : [" << failures << "] - duration: " << double(th_end_ms-th_start_ms)/1000 << std::endl;
}

void th_main_consumer( uint32_t th_num, uint32_t run_time, lock_free_queue* q )
{
  uint32_t     dit_pop;
  uint32_t     failures   = 0;
  uint32_t     successes  = 0;
  uint32_t     cycles     = 0;

  auto th_start_ms = utils::now<std::chrono::milliseconds>();

  //double last_mon  = th_start_ms;
  for (;;)
  {
    if ( q->pop(dit_pop) != core::result_t::eSuccess )
      ++failures;
    else
      ++successes;

    ++cycles;

    auto th_end_ms = utils::now<std::chrono::milliseconds>();
    if (double(th_end_ms-th_start_ms) >= run_time)
      break;
  }

  auto th_end_ms = utils::now<std::chrono::milliseconds>();

  std::osyncstream(std::cout) << "C TH [" <<  th_num <<  "] cycles : [" << cycles << "] - successes : [" << successes << "] - failures : [" << failures << "] - duration: " << double(th_end_ms-th_start_ms)/1000 << std::endl;
}

void th_main_monitor( status_queue* mon, uint32_t mon_time, uint32_t run_time, lock_free_queue* q )
{
  auto th_start_ms = utils::now<std::chrono::milliseconds>();
  

  double last_mon  = th_start_ms;
  for (;;)
  {
    auto   current_ms = utils::now<std::chrono::milliseconds>();
    if (double(current_ms-th_start_ms) >= run_time)
      break;

    if (( current_ms-last_mon ) >= mon_time)
    {
      last_mon = current_ms;

      double    _timestamp = double(current_ms-th_start_ms);
      uint64_t  _cur_size  = q->size();

      mon->push( queue_status_t(_timestamp, _cur_size) );
      std::osyncstream(std::cout) << "M TH [-] - timestamp: " << _timestamp/1000 << " - size: " << _cur_size << std::endl;
    }

  }

  auto th_end_ms = utils::now<std::chrono::milliseconds>();

  std::osyncstream(std::cout) << "M TH [-] - duration: " << double(th_end_ms-th_start_ms)/1000 << std::endl;
}

/**
 * The following program make use of arena_allocator template.
 * 
 */
int main( int argc, const char* argv[] )
{ 
  (void)argc;
  (void)argv;

  lock_free_queue queue;
  status_queue    mon_queue;

  uint32_t value = 10;
  queue.push(value);
  queue.push(20);


  std::cout  << (int)queue.lock() << std::endl;

  return 0;

/*
  uint32_t  value;

  queue.status();
  
  std::cout << "1->PUSH(100)\n";
  queue.push( 100 );
  std::cout << "2->PUSH(200)\n";
  queue.push( 200 );

  queue.status();

  queue.pop(value);
  std::cout << "3->POP(expected 100) - value = " << value << std::endl;

  queue.pop(value);
  std::cout << "4->POP(expected 200) - value = " << value << std::endl;
  
  queue.status();

  std::cout << "5->PUSH(300)\n";
  queue.push( 300 );

  queue.status();

  queue.pop(value);
  std::cout << "6->POP(expected 300) - value = " << value << std::endl;

  queue.status();

  if ( queue.pop(value) == false )
  {
    std::cout << "7->POP() - failed as expected = " << std::endl; 
  }else{
    std::cout << "7->POP() - error - read value = " << value << std::endl;
  }

  queue.status();

  return 0;
*/
  uint32_t producers      = 1;
  uint32_t consumers      = 5;
  uint32_t mon_time_ms    = 100;
  uint32_t run_time_ms    = 10000;       // milliseconds

  std::vector<std::thread>  vec_producers;
  std::vector<std::thread>  vec_consumers;

  std::thread th_mon( th_main_monitor, &mon_queue, mon_time_ms, run_time_ms, &queue );

  for ( uint32_t th_num = 0; th_num < producers; th_num++ )
  {
    vec_producers.emplace_back( std::thread( th_main_producer, th_num, run_time_ms, &queue) );
  }

  for ( uint32_t th_num = 0; th_num < consumers; th_num++ )
  {
    vec_consumers.emplace_back( std::thread( th_main_consumer, th_num, run_time_ms, &queue) );
  }

  for ( auto& th : vec_producers )
    th.join();

  for ( auto& th : vec_consumers )
    th.join();

  th_mon.join();

  std::osyncstream(std::cout) << "NOT CONSUMED ITEMS = " << queue.size() << std::endl;
  std::osyncstream(std::cout) << std::endl;
  std::osyncstream(std::cout) << "----------------------------" << std::endl;
  std::osyncstream(std::cout) << std::endl;
  while ( !mon_queue.empty() )
  {
    std::osyncstream(std::cout) << "M TH [-] - timestamp: " << mon_queue.front()._timestamp/1000 << " - size: " << mon_queue.front()._size << std::endl;
    mon_queue.pop();
  }

  return 0;
}