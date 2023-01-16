#include <iostream>
#include <thread>
#include <chrono>

#include <core/stop_watch.h>
 
 
int main()
{
  core::stop_watch<std::chrono::nanoseconds,true>  swAutoInit;
  core::stop_watch<std::chrono::nanoseconds>       swNoInit;

  swNoInit.reset();

  std::this_thread::sleep_for(1s);

  std::cout << swAutoInit.peek() << std::endl;
  std::cout << swNoInit.peek() << std::endl;

  return 0;
}