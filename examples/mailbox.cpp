
#include <iostream>
#include <syncstream>
#include <assert.h>

#include "core/utils.h"
#include "mailbox.h"


class mbx_data
{
public:
  mbx_data()
    : _value(0)
  {
    std::osyncstream(std::cout) << "create mbx_data " << std::endl;
  }

  mbx_data( const mbx_data& md )
    : _value(md._value)
  {
    std::osyncstream(std::cout) << "create mbx_data from copy constructor" << std::endl;
  }

  mbx_data( mbx_data&& md )
    : _value(md._value)
  {
    std::osyncstream(std::cout) << "create mbx_data from move constructor" << std::endl;
  }

  ~mbx_data()
  {
    std::osyncstream(std::cout) << "destroy mbx_data " << std::endl;
  }

  
  mbx_data& operator=( const mbx_data& md ) 
  { 
    std::osyncstream(std::cout) << "assigned value by copy" << std::endl;
    _value = md._value; 
    return *this;
  }

  
  mbx_data& operator=( mbx_data&& md ) 
  { 
    std::osyncstream(std::cout) << "assigned value by move" << std::endl;
    _value = md._value; 
    return *this;
  }

  void set_value( uint32_t value )
  { _value = value; }

  uint32_t get_value() const
  { return _value; }

  uint32_t _value;

};

using mailbox_type = lock_free::mailbox<mbx_data, core::ds_impl_t::lockfree, 0>;

void th_main_write( mailbox_type* mbx, [[maybe_unused]] uint32_t run_time_ms )
{
  auto th_start_ms = core::utils::now<std::chrono::milliseconds>();
  mbx_data  md;
  uint32_t  cnt = 0;
  while (true)
  {
    std::this_thread::sleep_for(500ms);

    md.set_value( cnt++ );
    mbx->write( md );

    auto th_end_ms = core::utils::now<std::chrono::milliseconds>();
    if (double(th_end_ms-th_start_ms) >= run_time_ms)
      break;    
  }
}

void th_main_read ( mailbox_type* mbx, [[maybe_unused]] uint32_t run_time_ms )
{
  auto th_start_ms = core::utils::now<std::chrono::milliseconds>();
  mbx_data data;
  while (true)
  {
    core::result_t res = mbx->read( data, 100 );
    switch (res)
    {
      case core::result_t::eEmpty :
        std::osyncstream(std::cout) << "core::result_t::eEmpty" << std::endl;
      break;

      case core::result_t::eSuccess :
        std::osyncstream(std::cout) << "core::result_t::eSuccess - got [" << data.get_value() << "]" << std::endl;
      break;

      case core::result_t::eTimeout :
        std::osyncstream(std::cout) << "core::result_t::eTimeout" << std::endl;
      break;

      case core::result_t::eNullPointer :
        std::osyncstream(std::cout) << "core::result_t::eNullPointer" << std::endl;
      break;
      
      case core::result_t::eDoubleFree :
        std::osyncstream(std::cout) << "core::result_t::eDoubleFree" << std::endl;
      break;

      default:
        std::osyncstream(std::cout) << "? handled by default: ? [" << (uint32_t)res << "]" << std::endl;
      break;
    }

    auto th_end_ms = core::utils::now<std::chrono::milliseconds>();
    if (double(th_end_ms-th_start_ms) >= run_time_ms)
      break;       
  }

}


int main()
{
  core::node_t<mbx_data,false,true,false>   nodo;


  mailbox_type  mbx( "test mbx" );

  std::cout << "MBX NAME [" << mbx.name() << "]" << std::endl;
  std::cout << "  - Initial size = " << mbx.size() << std::endl;
  std::cout << "  - Is Empty()   = " << (mbx.empty()?"true":"false") << std::endl;

  uint32_t writers      = 1;
  uint32_t readers      = 1;
  uint32_t run_time_ms  = 60000;       // milliseconds

  std::vector<std::thread>  vec_writers;
  std::vector<std::thread>  vec_readers;

  for ( uint32_t th_num = 0; th_num < writers; th_num++ )
  {
    vec_writers.emplace_back( std::thread( th_main_write, &mbx, run_time_ms ) );
  }

  for ( uint32_t th_num = 0; th_num < readers; th_num++ )
  {
    vec_readers.emplace_back( std::thread( th_main_read , &mbx, run_time_ms ) );
  }

  for ( auto& th : vec_writers )
    th.join();

  for ( auto& th : vec_readers )
    th.join();

  return 0;
}