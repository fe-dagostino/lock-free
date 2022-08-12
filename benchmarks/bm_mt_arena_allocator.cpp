#include <iostream>
#include <string>

#include "arena_allocator.h"
#include "core/arena_allocator.h"

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
  
  data_item_t() noexcept
  {
    for ( u_int32_t i = 0; i < 12; i++ )
      data[i]=i;
  }

  data_item_t( u_int32_t value ) noexcept
  {
    for ( u_int32_t i = 0; i < 12; i++ )
      data[i]=value;
  }

  ~data_item_t() noexcept
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

  if (argc < 2)
  {
    std::cout << "valid options:" << std::endl;
    std::cout << " system            - will use 'new' and 'delete'" << std::endl;
    std::cout << " core              - will use core::arena_allocator" << std::endl;
    std::cout << " lock-free         - will use lock-free::arena_allocator" << std::endl;
    return 0;
  }
  
  const std::string c_option               = argv[1];
  const uint32_t    c_th_max               = 16;
  const uint32_t    c_pre_items            = 1000000;
  const uint32_t    c_max_repeat           = 50;
  double            c_th_results[c_th_max][c_th_max];

  lock_free::arena_allocator<data_item_t,u_int32_t,c_pre_items,c_pre_items*c_th_max, 0, 0 >   arena_lock_free;
  core::arena_allocator<data_item_t,u_int32_t,c_pre_items,c_pre_items*c_th_max, 0, 0 >        arena_mutex;

  data_item_t** arrItems = new data_item_t* [c_th_max*c_pre_items];

  std::thread* th_handle[c_th_max];

//---------------------------------------
  std::string bm_target = ( c_option == "system" )?"] - Using new and delete - ":( c_option == "core" )?"] - Using Core arena_allocator - ":"] - Using Lock-Free arena_allocator - ";

  for ( uint32_t th_count = 1; th_count <= c_th_max; ++th_count )
  {
    uint32_t    adj_pre_items = c_pre_items/th_count;

    for ( uint32_t th_ndx = 0; th_ndx < th_count; ++th_ndx )
    {
      // Start thread      
      th_handle[th_ndx] = new std::thread( [&]( uint32_t thread_ndx ){
            
            auto th_start_ms = utils::now<std::chrono::milliseconds>();

              uint32_t    fail_count    = 0;
              std::string bm_title      = "Max Threads [" + std::to_string(th_count) + "] TH Index [" + std::to_string(thread_ndx) + bm_target;
              
            
              for ( uint32_t repeat_counter = 0; repeat_counter < c_max_repeat; ++repeat_counter )
              {
                if ( c_option == "system" )
                {
                  for ( u_int32_t i = 0; i < adj_pre_items; ++i )
                  {
                    arrItems[(thread_ndx*c_pre_items)+i] = new data_item_t(i);
                    if ( arrItems[(thread_ndx*c_pre_items)+i] == nullptr )
                    { ++fail_count; }                  
                  }

                  for ( u_int32_t i = 0; i < adj_pre_items; ++i )
                  {
                    if ( arrItems[(thread_ndx*c_pre_items)+i] != nullptr )
                    { delete arrItems[(thread_ndx*c_pre_items)+i]; }
                  }
                } 
                else if ( c_option == "core")
                {
                  for ( u_int32_t i = 0; i < adj_pre_items; ++i )
                  {
                    arrItems[(thread_ndx*c_pre_items)+i] = arena_mutex.allocate(i);
                    if ( arrItems[(thread_ndx*c_pre_items)+i] == nullptr )
                    { ++fail_count; }
                  }

                  for ( u_int32_t i = 0; i < adj_pre_items; ++i )
                  { 
                    if ( arrItems[(thread_ndx*c_pre_items)+i] != nullptr )
                    { (void)arena_mutex.deallocate(arrItems[(thread_ndx*c_pre_items)+i]); }
                  }
                }
                else //if ( c_option == "lock-free" )
                {
                  for ( u_int32_t i = 0; i < adj_pre_items; ++i )
                  {
                    arrItems[(thread_ndx*c_pre_items)+i] = arena_lock_free.allocate(i);
                    if ( arrItems[(thread_ndx*c_pre_items)+i] == nullptr )
                    { ++fail_count; }
                  }

                  for ( u_int32_t i = 0; i < adj_pre_items; ++i )
                  {
                    if ( arrItems[(thread_ndx*c_pre_items)+i] != nullptr )
                    { (void)arena_lock_free.deallocate(arrItems[(thread_ndx*c_pre_items)+i]); }
                  }
                }
                
              }

            auto th_end_ms = utils::now<std::chrono::milliseconds>();
            
            c_th_results[th_count-1][thread_ndx] = double(th_end_ms-th_start_ms)/1000;

            std::cout <<  bm_title.c_str() << "failures : " << fail_count << " - duration: " << c_th_results[th_count-1][thread_ndx] << std::endl;

      }, th_ndx );
    }

    for ( uint32_t th_ndx = 0; th_ndx < th_count; ++th_ndx )
    {
      th_handle[th_ndx]->join();
    }

    for ( uint32_t th_ndx = 0; th_ndx < th_count; ++th_ndx )
    {
      std::cout << c_th_results[th_count-1][th_ndx] << " ";
    }
    std::cout << std::endl;

  }
  
  std::cout << std::endl;
  std::cout << std::endl;

  for ( uint32_t th_count = 1; th_count <= c_th_max; ++th_count )
  {
    for ( uint32_t th_ndx = 0; th_ndx < th_count; ++th_ndx )
    {
      std::cout << c_th_results[th_count-1][th_ndx] << " ";
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;

  return 0;
}