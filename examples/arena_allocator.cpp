#include <iostream>

#include "arena_allocator.h"



struct data_item_t {
  
  constexpr data_item_t() noexcept
  {
    printf("data_item_t()\n");
  }

  constexpr data_item_t( u_int32_t value ) noexcept
  {
    data = value;
    printf("data_item_t( %u )\n", value );
  }

  constexpr ~data_item_t() noexcept
  {
    printf("~data_item_t( %u )\n", data );
  }

  u_int32_t  data;
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
  lock_free::arena_allocator<data_item_t,u_int32_t,c_pre_items>   allocator;
  std::cout << "End   initialization: arena_allocator" << std::endl;
  
  std::cout << std::endl;
  
  std::cout << "type_size()  : " << allocator.type_size()  << std::endl;
  std::cout << "length()     : " << allocator.length()     << std::endl;
  std::cout << "max_length() : " << allocator.max_length() << std::endl;
  std::cout << "capacity()   : " << allocator.capacity()   << std::endl;
  std::cout << "max_size()   : " << allocator.max_size()   << std::endl;
  
  std::cout << std::endl;
  std::cout << std::endl;

  data_item_t*  pDataItem_1 = allocator.allocate( 10 );
  data_item_t*  pDataItem_2 = allocator.allocate( 42 );

  std::cout << std::endl;
  std::cout << std::endl;

  std::cout << "length()     : " << allocator.length()     << std::endl;
  std::cout << std::endl;
  std::cout << "data_item_1  : " << pDataItem_1->data      << std::endl;
  std::cout << "data_item_2  : " << pDataItem_2->data      << std::endl;

  std::cout << std::endl;
  std::cout << std::endl;

  std::cout << "deallocate data_item_1 calling allocator.deallocate()" << std::endl;
  allocator.deallocate(pDataItem_1);

  std::cout << "WE DO NOT deallocate data_item_2" << std::endl;
  std::cout << "data_item_2 destructor will be automatically invoked by the arena_allocator." << std::endl;

  return 0;
}