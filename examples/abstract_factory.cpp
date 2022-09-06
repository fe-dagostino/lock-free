#include <iostream>
#include <chrono>
#include <assert.h>

#include "core/abstract_factory.h"
#include "core/unique_ptr.h"

using namespace std::chrono_literals;

class parameter
{

};

parameter _null_param;

class base_class
{
private:
  /* data */
public:
  base_class()
   : _param(_null_param)
  {}

  base_class( const std::string& message, parameter& param )
    : _message(message), _param(param)
  {}

  virtual ~base_class()
  {}

  virtual std::string get_name() 
  {  return "base_class"; }

  std::string _message;
  parameter&  _param;
};

class derived_0 : public base_class, public core::plug_name<"derived_0">
{
public:
  derived_0()
  {}

  derived_0( const std::string& message, parameter& param )
    : base_class( message, param)
  {}

  ~derived_0()
  {}

  virtual std::string get_name() override
  {  return "derived_0"; }

 
};

class derived_1 : public base_class, public core::plug_name<"derived_1">
{
public:
  derived_1()
  {}

  derived_1( const std::string& message, parameter& param )
    : base_class(message, param)
  {}  

  ~derived_1()
  {}

  virtual std::string get_name() override
  {  return "derived_1"; }

};

/**
 * The following program make use of abstract_factory template.
 * 
 */
int main( [[maybe_unused]] int argc, [[maybe_unused]] const char* argv[] )
{ 
  // no default no parameters
  {
    using factory = core::abstract_factory< base_class, std::nullptr_t, 
                                            derived_0, 
                                            derived_1>; 

    std::unique_ptr<base_class> _ptrBase     = factory::create("undefined");
    std::unique_ptr<base_class> _ptrDerived0 = factory::create("derived_0");
    std::unique_ptr<base_class> _ptrDerived1 = factory::create("derived_1");

    assert( _ptrBase                 == nullptr      );
    assert( _ptrDerived0->get_name() == "derived_0"  );
    assert( _ptrDerived1->get_name() == "derived_1"  );
  }

  // with default no parameters
  {
    using factory = core::abstract_factory< base_class, base_class, 
                                            derived_0, 
                                            derived_1>;

    std::unique_ptr<base_class> _ptrBase     = factory::create("undefined");
    std::unique_ptr<base_class> _ptrDerived0 = factory::create("derived_0");
    std::unique_ptr<base_class> _ptrDerived1 = factory::create("derived_1");

    assert( _ptrBase->get_name()     == "base_class" );
    assert( _ptrDerived0->get_name() == "derived_0"  );
    assert( _ptrDerived1->get_name() == "derived_1"  );
  }

  // with default and parameters
  {
    using factory = core::abstract_factory< base_class, base_class, 
                                            derived_0, 
                                            derived_1>;

    parameter                   _empty_param;

    std::unique_ptr<base_class> _ptrBase     = factory::create("undefined", "msg base"     , _empty_param );
    std::unique_ptr<base_class> _ptrDerived0 = factory::create("derived_0", "msg derived_0", _empty_param );
    std::unique_ptr<base_class> _ptrDerived1 = factory::create("derived_1", "msg derived_1", _empty_param );

    assert( _ptrBase->get_name()     == "base_class" );
    assert( _ptrDerived0->get_name() == "derived_0"  );
    assert( _ptrDerived1->get_name() == "derived_1"  );
  }

  std::cout << std::endl;
  std::cout << std::endl;

  return 0;
}