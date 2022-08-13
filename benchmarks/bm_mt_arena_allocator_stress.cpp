#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <iostream>
#include <syncstream>
#include <queue>

#include "arena_allocator.h"

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

struct queue_status_t {
  queue_status_t( const double& timestamp, const uint64_t& size )
   : _timestamp(timestamp), _size(size)
  {}

  double     _timestamp;
  uint64_t   _size;
};

using lock_free_arena = lock_free::arena_allocator<data_item_t,uint64_t, 10000, 10000, 0, 10000/3 >;
using status_queue    = std::queue<queue_status_t>;


static void th_main_alloc_dealloc( uint32_t th_num, uint32_t run_time, lock_free_arena* arena )
{
  uint32_t                  allocation_failures    = 0;
  uint32_t                  allocation_successes   = 0;
  uint32_t                  deallocation_failures  = 0;
  uint32_t                  deallocation_successes = 0;
  uint32_t                  cycles     = 0;
  
  std::queue<data_item_t*>  ptr_queue;      

  auto th_start_ms = utils::now<std::chrono::milliseconds>();

  for (;;)
  {
    if ( (std::rand() % 2) == 0 )
    {
      data_item_t* pItem = arena->allocate(th_num+1);
      if ( pItem == nullptr )
        ++allocation_failures;
      else
      {
        ++allocation_successes;
        ptr_queue.push(pItem); 
      }
    }
    else if (!ptr_queue.empty())
    {
      data_item_t* pItem = ptr_queue.front(); 
      ptr_queue.pop();

      if ( arena->deallocate(pItem) != core::return_type::eSuccess )
        ++deallocation_failures;
      else
        ++deallocation_successes;

    }
    
    ++cycles;

    auto th_end_ms = utils::now<std::chrono::milliseconds>();
    if (double(th_end_ms-th_start_ms) >= run_time)
      break;
  }

  auto th_end_ms = utils::now<std::chrono::milliseconds>();

  std::osyncstream(std::cout) << "P TH [" <<  th_num    << "] cycles : [" << cycles << "] - alloc - OK : [" 
                              << allocation_successes   << "] - FAIL : [" << allocation_failures << "] - dealloc - OK : [" 
                              << deallocation_successes << "] - FAIL : [" << deallocation_failures 
                              << "] - duration: " << double(th_end_ms-th_start_ms)/1000 << std::endl;
}


void th_main_monitor( status_queue* mon, uint32_t mon_time, uint32_t run_time, lock_free_arena* arena )
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
      uint64_t  _cur_size  = arena->length();

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

  lock_free_arena arena;
  status_queue    mon_queue;

  std::osyncstream(std::cout) << "INITIAL ARENA STATUS:" 
                              << "\n - length()     = " << arena.length()     
                              << "\n - max_length() = " << arena.max_length() 
                              << "\n - capacity()   = " << arena.capacity()   
                              << "\n - max_size()   = " << arena.max_size()   << std::endl;

  uint32_t nb_threads     = 12;
  uint32_t mon_time_ms    = 1000;
  uint32_t run_time_ms    = 30000;       // milliseconds

  std::vector<std::thread>  vec_alloc_dealloc;

  std::thread th_mon( th_main_monitor, &mon_queue, mon_time_ms, run_time_ms, &arena );

  for ( uint32_t th_num = 0; th_num < nb_threads; th_num++ )
  {
    vec_alloc_dealloc.emplace_back( std::thread( th_main_alloc_dealloc, th_num, run_time_ms, &arena) );
  }

  for ( auto& th : vec_alloc_dealloc )
    th.join();

  th_mon.join();

  std::osyncstream(std::cout) << "FINAL ARENA STATUS:" 
                              << "\n - length()     = " << arena.length()     
                              << "\n - max_length() = " << arena.max_length() 
                              << "\n - capacity()   = " << arena.capacity()   
                              << "\n - max_size()   = " << arena.max_size()   << std::endl;

  
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