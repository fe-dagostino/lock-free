#include <iostream>
#include <chrono>

#include "core/arena_allocator.h"
#include "core/mem_unique_ptr.h"

using namespace std::chrono_literals;

struct data_item_t {
 
  data_item_t() noexcept
  {
    printf("data_item_t()\n");
  }

  data_item_t( uint32_t value ) noexcept
  {
    data = value;
    printf("data_item_t( %u )\n", value );
  }

  ~data_item_t() noexcept
  {
    printf("~data_item_t( %u )\n", data );
  }

  uint32_t  data;
};


/**
 * The following program make use of arena_allocator template.
 * 
 */
int main( int argc, const char* argv[] )
{ 
  (void)argc;
  (void)argv;

  const uint32_t c_pre_items = 10;

  std::cout << "Start initialization: arena_allocator" << std::endl;
  core::arena_allocator<
                              data_item_t,          // user data type
                              uint32_t,            // data type for all counters
                              c_pre_items,          // chunk_size, how many data_item_t in a single chunck of memory.
                              c_pre_items,          // initial_size, number of items to reserve when object is instantiated. 
                              0,                    // 0 means that size can grow until there is available memory 
                              5                     // this is the threashold used to trigger backgroud thread, basically
                                                    // when number of available items is <= 5 a new chunk will be allocated 
                                                    // background.        
                              >   allocator;
  std::cout << "End   initialization: arena_allocator" << std::endl;
  
  std::this_thread::sleep_for(2000ms);

  std::cout << std::endl;
  
  std::cout << "type_size()  : " << allocator.type_size()  << std::endl;
  std::cout << "length()     : " << allocator.length()     << std::endl;
  std::cout << "max_length() : " << allocator.max_length() << std::endl;
  std::cout << "capacity()   : " << allocator.capacity()   << std::endl;
  std::cout << "max_size()   : " << allocator.max_size()   << std::endl;
  
  std::cout << std::endl;
  std::cout << std::endl;

  data_item_t*  pDataItem_1 = allocator.allocate( 1 );
  data_item_t*  pDataItem_2 = allocator.allocate( 2 );

  std::cout << std::endl;
  std::cout << std::endl;

  std::cout << "length()     : " << allocator.length()     << std::endl;
  std::cout << std::endl;
  std::cout << "data_item_1  : " << pDataItem_1->data      << std::endl;
  std::cout << "data_item_2  : " << pDataItem_2->data      << std::endl;

  std::cout << std::endl;
  std::cout << std::endl;

  std::cout << " ----------------- " << std::endl;
  std::cout << "max_length() : " << allocator.max_length() << std::endl;
  data_item_t*  pDataItem_3 = allocator.allocate( 3 );
  data_item_t*  pDataItem_4 = allocator.allocate( 4 );
  data_item_t*  pDataItem_5 = allocator.allocate( 5 );
  data_item_t*  pDataItem_6 = allocator.allocate( 6 );  // trigger allocator thread.

  std::cout << "Wait 1000 ms in order to be sure that background thread allocated a new chuck : " << std::endl;
  std::this_thread::sleep_for(1000ms);
  std::cout << " Read new max_length() " << std::endl;
  std::cout << "max_length() : " << allocator.max_length() << std::endl;
  (void)allocator.deallocate(pDataItem_3);
  (void)allocator.deallocate(pDataItem_4);
  (void)allocator.deallocate(pDataItem_5);
  (void)allocator.deallocate(pDataItem_6);
  std::cout << " ----------------- " << std::endl;

  // Create a pointer to data_item_t not managed by arena_allocator;
  std::cout << "Create a pointer to data_item_t ( pDataItem_ext ) not managed by arena_allocator" << std::endl;
  data_item_t*  pDataItem_ext = new data_item_t(404);
  std::cout << std::endl;
  std::cout << "calling allocator.is_valid( pDataItem_ext )" << std::endl;
  std::cout << " -- result = " << (allocator.is_valid(pDataItem_ext)?"TRUE":"FALSE") << std::endl;
  std::cout << "calling allocator.is_valid( pDataItem_1 )" << std::endl;
  std::cout << " -- result = " << (allocator.is_valid(pDataItem_1)?"TRUE":"FALSE") << std::endl;
  std::cout << "calling allocator.is_valid( pDataItem_2 )" << std::endl;
  std::cout << " -- result = " << (allocator.is_valid(pDataItem_2)?"TRUE":"FALSE") << std::endl;
  std::cout << std::endl;
  std::cout << "Destroy ( pDataItem_ext ) instance." << std::endl;
  delete pDataItem_ext;
  
  std::cout << std::endl;
  std::cout << std::endl;

  std::cout << "deallocate data_item_1 calling allocator.deallocate()" << std::endl;
  (void)allocator.deallocate(pDataItem_1);

  std::cout << "WE DO NOT deallocate data_item_2" << std::endl;
  std::cout << "data_item_2 destructor will be automatically invoked by the arena_allocator." << std::endl;

  std::cout << std::endl;
  std::cout << std::endl;

  return 0;
}