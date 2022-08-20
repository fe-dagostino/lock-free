
#include <iostream>

#include "core/mem_unique_ptr.h"

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
  core::mem_unique_ptr<test_unique_ptr>  _ptr0 = new test_unique_ptr( "from raw pointer" );
  std::cout <<  "_ptr0.auto_delete() = " << _ptr0.auto_delete() << std::endl;

  core::mem_unique_ptr<test_unique_ptr>  _ptr1 = std::move(_ptr0);
  std::cout <<  "_ptr1.auto_delete() = " << _ptr1.auto_delete() << std::endl;

  std::cout <<  std::endl; 
  std::cout <<  std::endl; 
  core::mem_unique_ptr<test_unique_ptr>  _ptr2;
  std::cout <<  "_ptr1.get() = " << _ptr1.get() << " - _ptr1.auto_delete() = " << _ptr1.auto_delete() << std::endl;
  std::cout <<  "_ptr2.get() = " << _ptr2.get() << " - _ptr2.auto_delete() = " << _ptr2.auto_delete() << std::endl;
  _ptr2 = std::move(_ptr1);
  std::cout <<  "_ptr2 = std::move(_ptr1);" << std::endl;
  std::cout <<  "_ptr1.get() = " << _ptr1.get() << " - _ptr1.auto_delete() = " << _ptr1.auto_delete() << std::endl;
  std::cout <<  "_ptr2.get() = " << _ptr2.get() << " - _ptr2.auto_delete() = " << _ptr2.auto_delete() << std::endl;

  std::cout <<  std::endl; 
  std::cout <<  std::endl; 
  core::mem_unique_ptr<test_unique_ptr>  _ptr4 = std::make_unique<test_unique_ptr>( "from std::unique_ptr pointer" );
  std::cout <<  "_ptr4.get() = " << _ptr4.get() << " - _ptr4.auto_delete() = " << _ptr4.auto_delete() << std::endl;
  std::cout <<  std::endl; 
  std::cout <<  std::endl; 
  std::unique_ptr<test_unique_ptr>       _ptr5 = std::make_unique<test_unique_ptr>( "from std::unique_ptr pointer for assignment" );
  core::mem_unique_ptr<test_unique_ptr>  _ptr6;
  std::cout <<  "_ptr5.get() = " << _ptr5.get() << std::endl;
  std::cout <<  "_ptr6.get() = " << _ptr6.get() << " - _ptr6.auto_delete() = " << _ptr6.auto_delete() << std::endl;
  _ptr6 = std::move(_ptr5);
  std::cout <<  "_ptr6 = std::move(_ptr5);" << std::endl;
  std::cout <<  "_ptr5.get() = " << _ptr5.get() << std::endl;
  std::cout <<  "_ptr6.get() = " << _ptr6.get() << " - _ptr6.auto_delete() = " << _ptr6.auto_delete() << std::endl;



  return 0;
}