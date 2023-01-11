#include <iostream>

#include "core/mutex.h"
#include "core/singleton.h"


// This class in only intended to show an example on how to extend
// core::singleton_t<>
class StdOutput final : public core::singleton_t<StdOutput>
{
  // This is required since StdOutput() constructor is protected.
  friend class core::singleton_t<StdOutput>;
protected:
  StdOutput()
  {
     log( "(1) StdOutput()" );
  }
public:
  ~StdOutput()
  {
     log( "(4) ~StdOutput()" );
  }
protected:
  constexpr void on_initialize() noexcept 
  { log( "(2) on_initialize()" ); }
  constexpr void on_finalize()   noexcept 
  { log( "(3) on_finalize()" ); }
public:
  void log( const std::string& msg ) noexcept
  {
    std::lock_guard<std::mutex>    _mtx(m_mtx);
    std::cout << msg << std::endl;
  }

private:
  std::mutex       m_mtx;

};

/**
 * The following program make use of singleton template.
 * This is a sample program to show the flow of calls and to check the interface also.
 */
int main( [[maybe_unused]] int argc, [[maybe_unused]] const char* argv[] )
{ 
  if ( StdOutput::is_valid() == true )
  {
    std::cerr << "Singleton cannot be valid before initialization" << std::endl;
    return -1;     
  }

  if ( StdOutput::initialize() == false )
  {
    std::cerr << "Failed to initialize singleton" << std::endl;
    return -2;     
  }

  if ( StdOutput::initialize() == false )
  {
    StdOutput::get_instance()->log( "singleton already initialized" );
  }

  if ( StdOutput::is_valid() == true )
  {
    StdOutput::get_instance()->log( "singleton is valid" );
  }

  // Just spawn 3 threads to log a message 
  std::thread th1( [](){ StdOutput::get_instance()->log( "message from thread 1" ); } );
  std::thread th2( [](){ StdOutput::get_instance()->log( "message from thread 2" ); } );
  std::thread th3( [](){ StdOutput::get_instance()->log( "message from thread 3" ); } );
  
  // wait for thread terminations
  th1.join();
  th2.join();
  th3.join();
  
  // Finalize the singleton.
  StdOutput::get_instance()->finalize();

  return 0;
}