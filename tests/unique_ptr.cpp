
#include <iostream>
#include <assert.h>

#include "core/unique_ptr.h"

class test_unique_ptr
{
private:
  /* data */
public:
  test_unique_ptr( const std::string& text )
    : _text(text)
  {
    std::cout << "create test_unique_ptr - " << _text << std::endl;
  }

  ~test_unique_ptr()
  {
    std::cout << "destroy test_unique_ptr - " << _text << std::endl;
  }
  const std::string _text;
};


int main()
{
  core::unique_ptr<test_unique_ptr>  _ptr0 = new test_unique_ptr( "from raw pointer" );
  std::cout <<  "_ptr0.auto_delete() = " << _ptr0.auto_delete() << std::endl;
  assert( _ptr0.get() != nullptr );

  core::unique_ptr<test_unique_ptr>  _ptr1 = std::move(_ptr0);
  std::cout <<  "_ptr1.auto_delete() = " << _ptr1.auto_delete() << std::endl;
  assert( _ptr1.get() != nullptr );

  std::cout <<  std::endl; 
  std::cout <<  std::endl; 
  core::unique_ptr<test_unique_ptr>  _ptr2;
  std::cout <<  "_ptr1.get() = " << _ptr1.get() << " - _ptr1.auto_delete() = " << _ptr1.auto_delete() << std::endl;
  std::cout <<  "_ptr2.get() = " << _ptr2.get() << " - _ptr2.auto_delete() = " << _ptr2.auto_delete() << std::endl;
  _ptr2 = std::move(_ptr1);
  std::cout <<  "_ptr2 = std::move(_ptr1);" << std::endl;
  std::cout <<  "_ptr1.get() = " << _ptr1.get() << " - _ptr1.auto_delete() = " << _ptr1.auto_delete() << std::endl;
  std::cout <<  "_ptr2.get() = " << _ptr2.get() << " - _ptr2.auto_delete() = " << _ptr2.auto_delete() << std::endl;
  assert( _ptr1.get() == nullptr );
  assert( _ptr2.get() != nullptr );


  std::cout <<  std::endl; 
  std::cout <<  std::endl; 
  core::unique_ptr<test_unique_ptr>  _ptr3 = std::make_unique<test_unique_ptr>( "from std::unique_ptr pointer" );
  std::cout <<  "_ptr3.get() = " << _ptr3.get() << " - _ptr3.auto_delete() = " << _ptr3.auto_delete() << std::endl;
  assert( _ptr3.get() != nullptr );


  std::cout <<  std::endl; 
  std::cout <<  std::endl; 
  std::unique_ptr<test_unique_ptr>       _ptr4 = std::make_unique<test_unique_ptr>( "from std::unique_ptr pointer for assignment" );
  core::unique_ptr<test_unique_ptr>  _ptr5;
  std::cout <<  "_ptr4.get() = " << _ptr4.get() << std::endl;
  std::cout <<  "_ptr5.get() = " << _ptr5.get() << " - _ptr5.auto_delete() = " << _ptr5.auto_delete() << std::endl;
  _ptr5 = std::move(_ptr4);
  std::cout <<  "_ptr5 = std::move(_ptr4);" << std::endl;
  std::cout <<  "_ptr4.get() = " << _ptr4.get() << std::endl;
  std::cout <<  "_ptr5.get() = " << _ptr5.get() << " - _ptr5.auto_delete() = " << _ptr5.auto_delete() << std::endl;
  assert( _ptr4.get() == nullptr );
  assert( _ptr5.get() != nullptr );

  return 0;
}