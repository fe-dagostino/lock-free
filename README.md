[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/language-c++-red.svg)](https://en.cppreference.com/)
# Lock-Free data structures

This repository will be populated with primarily with lock-free data structures, keeping implementation simple and hopefully readable for everyone, and with other useful data structures. 

Current data structures implementations leverage C++20, so in order to [build examples](#build-from-source) or to use such the following implementations a compiler that support at least C++20 is a pre-requirements. 

---
## Lock-Free implementations - namespece `lock_free`

Actually, the following lock-free data structures have been implemented:
* [arena_allocator](https://github.com/fe-dagostino/The-Magicians/blob/master/lock-free/arena_allocator/README.md) (*Lock-Free version*) : **`allocate()`** and **`deallocate()`** with a complexity of **`O(1)`**.
* ring-buffer 
* [queue](#queue) : this is a generic queue implementation that can be instantiated in one of the following forms: raw, mutex, spinlock, lockfree.
* [stack](#stack) : generic stack implementation with support for raw, mutex, spinlokc, lockfree working modes as for the queue.
* [multi-queue](#multi-queue): take advantage of both [queue](#queue) and arena_allocator implementation to minimize resource contention and consequently maximizing performances.
* [mailbox](#mailbox) : a mailbox implementation based on lock_free::queue and leveraging core::event for notifying writes.

---
### ring-buffer  **(*not finalize*)**

This implementation leverage the std::array a well specified amount of `slots` where to hold our data.

This data structure can be shared between multiple threads in both reading and writing operations since all operation in each single `slot` are synchronized.

This structure can be used in all circumstances where we don't care about the order of execution, this means that each thread can take a different time to process the single `data` unit. 

---
### queue

What makes this implementation peculiar is the fact that it hides 4 different implementations:
```cpp
// Create an instance of for raw queue, no thread safe
lock_free::queue<uint32_t,uint32_t,core::ds_impl_t::raw>          _queue_raw;

// Create an instance of for a queue protected with std::mutex
lock_free::queue<uint32_t,uint32_t,core::ds_impl_t::mutex>        _queue_with_mutex;

// Create an instance of for a queue protected with core::mutex
lock_free::queue<uint32_t,uint32_t,core::ds_impl_t::spinlock>     _queue_with_spinlock;

// Create an instance of for a lock-free queue
lock_free::queue<uint32_t,uint32_t,core::ds_impl_t::lockfree>     _queue_lock_free;
```

Each instance, so each specialisation, will contain only needed data members without extra cost in terms of memory or execution, so the RAW implementation will have the best performances since there are no synch mechanisms in such instance.

---
### stack
Exactly as for the [queue](#queue) the same class can be instantiated to leverage different implementations.


---
### multi-queue  

I guess there are ton of implementation for lock-free queue and this is one more.
This implementation can be used as:
- ***Single Queue***: shared between a 1-producer and 1-consumer as well as N-producer and M-Consumer. Each single queue is FIFO compliant. 
- ***Multiple Queues***: useful in each scenario where we have multiple producers (threads) and we can dedicate one thread to one queue and we have one or more consumer taking data from all of this queues. A tipical application is for logging activities, where logs are provided by different theads and there is one other in charge to serialize and store all informations. 
**Note**: FIFO is guaranteed on each single queue, but not between queues.
To leverage maximum performance from this implementation, it is necessary to use preallocated `nodes`.

A multi-queue class expose an interface compatible with lock_free::queue, moreover it allow also to selelct internal queue, so for example:

```
using queue_type = lock_free::multi_queue<u_int64_t,uint32_t, 8 /*number of queues*/>;

queue_type  mqueue;

// Invoking push() with only 1 parameter automatically select one 
// of the queue accordingly with the calling thread and enqueue on
// such selected queue.  
mqueue.push( 12 );

// Invoking push() with 2 parameters allow the application to 
// select on which queue, in the range [0..7 (queues-1)], the data should 
// be enqueued.
mqueue.push( 1, 12 );
```

It is important to notice that each queue has its own arena_allocator in order to reduce resources contention in favour of performances. 

---
### mailbox
Useful in circumstances where there is the need to exchanges data between producer and consumer without to have consumer/s continuously checking the queue. One typical application is for logging purpose, where there is the need to centralize logging, but in your application there are many thread producing log information, this is a perfect use case for a mailbox, since there is a minimal extra for each thread to call mailbox->write() and then one other thread will manage to read and physically write the log on disk, db, stream ... .
Implementation leverage lock_free::queue and core::event to create a mailbox where producer relies on queue implementation (lockfree, mutex ..) instead the consumer/s rely on condition variable and wake up periodically or when a signal occurs.
For a working example please refer to `examples` subfolder for [mailbox.h](./examples/mailbox.cpp).

```cpp

/**
 * This is an example class. You can avoid creating copy and move constructors,
 * but in such case the compiler will use default constructor and assignment operator.
 * In order to fully leverage the move semantic you should create both move constructor and
 * assignment operator with move semantic.
*/
class mbx_data
{
public:
  mbx_data()
    : _value(0)
  {  }

  ...
  .....

  void set_value( uint32_t value )
  { _value = value; }

  uint32_t get_value() const
  { return _value; }

private:
  uint32_t _value;
};

// all other available implementation can be used
using mailbox_type = lock_free::mailbox<mbx_data, core::ds_impl_t::lockfree, 0>;

// Producer main thread with writing logic. 
// The mailbox can be shared among different threads.
void th_main_write( mailbox_type* mbx )
{
  mbx_data  md;
  while ( true )
  {
    md.set_value( .. );

    mbx->write( md );
  }
}

// The mailbox allow multiple consumers as well.
void th_main_read( mailbox_type* mbx )
{
  mbx_data data;
  while ( true )
  {
    // Read the value or wait 100 ms.
    // During the waiting time a signal can occur then the read will 
    // unlock for reading. 
    core::result_t res =  mbx->read( data, 100 );
    ...
    .....
  }
}


```


---
## Other implementations - namespece `core`

* [arena_allocator](https://github.com/fe-dagostino/The-Magicians/blob/master/lock-free/arena_allocator/README.md) (*Spinlock version*): **`allocate()`** and **`deallocate()`** with a complexity of **`O(1)`**.
* memory_address ...
* mem_unique_ptr ... *wip*
* mutex
* event
* abstract_factory: an implementation that make use of templates, metaprogramming, concepts and functional to create all at compile-time, since we know all information when we build our program.
* *type_traits* extensions in "types.h":
  * **conditional**: similar to `std::conditional_t`, the same pattern have been applied to values instead of types
  * **are_base_of**: extend `std::is_base_of` for multiple types
  * **derived_types** concept: leverage `is_base_of_types` to create a constraints
  * **mutex_interface** concept: constraints for a mutex interface.
  * **tstring_t**: literals as template parameters used by `plug_name<>`


# Build from source
In order to build the examples and benchmarks programs the following are required:

* a compiler with C++20 support
* cmake >= 3.16
* git (optional)

Source code can be retrieved either from cloning github repository with git or downloading .zip archive.

```cpp
# git clone https://github.com/fe-dagostino/libcsv.git
# cd libcsv
# mkdir build
# cmake ../ -DCMAKE_BUILD_TYPE=Release -DLF_BUILD_BENCHMARKS=ON -DLF_BUILD_EXAMPLES=ON -DLF_BUILD_TESTS=ON
# make
```

**Note**: currently the library is headers only, so except that for examples programs you don't need to build it, but just to include necessary headers in your project.
