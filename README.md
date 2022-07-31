# Lock-Free data structures

This repository will be populated with primarily with lock-free data structures, keeping implementation simple and hopefully readable for everyone, and with other useful data structures. 

Current data structures implementations leverage C++20, so in order to build examples or to use such the following implementations a compiler that support at least C++20 is a pre-requirements. 

---
### Lock-Free implementations

Actually, the following lock-free data structures have been implemented:
* ring-buffer 
* multi-queue

### ring-buffer

This implementation leverage the std::array a well specified amount of `slots` where to hold our data.

This data structure can be shared between multiple threads in both reading and writing operations since all operation in each single `slot` are synchronized.

This structure can be used in all circumstances where we don't care about the order of execution, this means that each thread can take a different time to process the single `data` unit. 

### multi-queue

I guess there are ton of implementation for lock-free queue and this is one more.
This implementation can be used as:
- ***Single Queue***: shared between a 1-producer and 1-consumer as well as N-producer and M-Consumer. Each single queue is FIFO compliant. 
- ***Multiple Queues***: useful in each scenario where we have multiple producers (threads) and we can dedicate one thread to one queue and we have one or more consumer taking data from all of this queue. 
**Note**: FIFO is guaranteed on each single queue, but not between queues.
To leverage maximum performance from this implementation, it is necessary to use preallocated `nodes`.

---
## Other implementations

### arena_allocator

An arena_allocator implementation that keep both alloc() and dealloc() to a complexity of O(1).
 
This arena_allocator is useful when :
 - in a program there is an extensive use of "new" and "delete" for a well-defined "data type";
 - we want to avoid memory fragmentation, since the program is supposed to run for long time without interruptions;
 - performances in our program are important and we want to avoid waste of CPU cycles.

Let's see how it works and how it is possible to keep allocation and deallocation with a complexity of O(1).

In the following [Figure 1] there is representation for the *arena_allocator*; In this specific example we are considering `chuck_size = 5` since we want to illustrate the mechanism and 5 memory_slot are more than enough for this scope. 

![Figure 1](.resources/arena_allocator_initial.svg)

As we can see in [Figure 1], there are different elements:
* **MEMORY CHUNK**: in order to avoid massive call to `new` and `delete` there is a single allocation, one for each `chunks`, where the amount of memory is enough to store `chunk_size` items of type `data_t` plus some extra memory required to manage the structure. In fact, the drawback to have better performances, at least with this implementation, is that we need a pointer for each single `memory_slot`, so 32 bits or 64 bits depending on the CPU architecture. Moreover, we need to keep other two pointers, respectively `first` and `last`, used for integrity checks as well as for deallocating the memory once `arena_allocator` will be destroyed.
* **MEMORY SLOT**: each `memory_slot` is divide in two areas that are respectively reserved for `user_data` information and a *pointer* to next free `memory_slot`, this last when the `memory_chunk` is initialized will be initialized with the address of next contiguous `memory_slot`, but after that, so during operation, this value can contain any of the available address for memory slots, in any of the allocated `memory_chunk`. 
* **next_free** : a *pointer* to `memory_slot` that always contain the address of next free memory_slot to be released with a call to allocate(). If there are no available `memory_slot` value will be `nullptr`. 

Now lets see a minimalist code for `allocate()` and `deallocate()`:
```cpp
  data_t* arena_allocator::allocate( ) 
  { 
    memory_slot* pCurrSlot = _next_free;
    if ( pCurrSlot == nullptr )       // There are no free slot available, this can occur in
      return nullptr;                 // if our arena_allocator have been limited to a maximum 
                                      // amount of memory_slot (size_limit template parameter)
                                      // or in case the system  run out-of-memory.

    
    // When we still have free memory_slot then the following 3 steps are done.
    
    _next_free = pCurrSlot->next();   // STEP 1: set _next_free to the next() in the chain.
                                      //         with reference to Figure 1, if _next_free was
                                      //         referring to 'Slot_1', then we move it to 
                                      //         Slot_1->next then means 'Slot_2'
    
    pCurrSlot->_next = nullptr;       // STEP 2: set next pointer to nullptr since this 
                                      //         memory_slot is "in use" and 'ownership'
                                      //         is released to the user.     
    
    return pCurrSlot->prt();          // STEP 3: release a pointer to data_t to the caller
  }
```

The above method `allocate()` basically performs:
* 1 x initial check
* 2 x assignment respectively at STEP 1 and STEP 2

```cpp
  void arena_allocator::deallocate( data_t* userdata ) noexcept
  {
    assert( userdata != nullptr );

    slot_pointer  pSlot = memory_slot::slot_from_user_data(userdata);

    userdata->~data_t();              // invoke data_t() destructor like a call to delete()

    pSlot->_next = _next_free;        // STEP 1: setting _next to _next_free, so following
                                      //         the status after allocate() above, we have
                                      //         pSlot = Slot_1, pSlot->_next = nullptr and
                                      //         and _next_free = Slot_2; then after this 
                                      //         assignment we will have:
                                      //           pSlot->next = Slot_2  

    _next_free = pSlot;               // STEP 2: _next_free = Slot_1, so in this specific
                                      //         use case we roll-back to the original status.

  }
```
The above method `deallocate()` basically performs:
* 1 x subtraction done by memory_slot::slot_from_user_data()
* 2 x assignment respectively at STEP 1 and STEP 2

In the above example, there is no mention for `constructor` and `destructor` time, since this are `data_t` dependent and then in charge of the user in any case. Moreover, both constructor and destructor are executed out of any synchronization mechanism, so do not have impact on arena_allocator performances in a concurrent environment.

The above function are simplified, since there is no syncrhonization for the full implementation, please refer to [arena_allocator.h](./include/arena_allocator.h) where there are two different methods for both `allocate()` and `deallocate()`, one version is thread safe the other one is unsafe, then synch is in charge to the user. 

Here some benckmark obtained running [bm_arena_allocator.cpp](./benchmarks/bm_arena_allocator.cpp). Compiled with `gcc 12.0.1 20220319`.

|               ns/op |                op/s |    err% |          ins/op |          cyc/op |    IPC |         bra/op |   miss% |     total | benchmark
|--------------------:|--------------------:|--------:|----------------:|----------------:|-------:|---------------:|--------:|----------:|:----------
|               29.90 |       33,445,193.47 |    0.1% |          233.62 |           68.81 |  3.395 |          53.62 |    0.0% |     13.49 | `Using new and delete`
|               21.69 |       46,108,483.97 |    0.2% |          186.00 |           49.91 |  3.727 |          43.00 |    0.0% |      9.80 | `Using arena_allocator`
|               13.38 |       74,764,961.41 |    0.3% |           36.00 |           30.77 |  1.170 |           3.00 |    0.0% |      6.03 | `Using arena_allocator unsafe`

The table show the difference between the classic `new` and `delete` compared with `allocate()` and `deallocate()` in the *arena_allocator* in both modes *safe* and *unsafe* modes.

In this specific benchmark we have only one thread allocating and deallocating memory, but when your program has many other calls to new and delete with different sizes, then running the program for hours will degrade `new` and `delete` performances when the arena_allocator keep performances constant in time.
There is nothing magic since the arena allocator leverage the fact that it has all `memory_slot` with the same size, instead the `new` and `delete` should deal with generic requests and even with `tcache` optimization and/or `buddy` implementations the `new` operation will cost more than O(1) to search a new slot for the user.

Now, just some details related to the implementation. The aim was to create a ***lock-free*** arena_allocator, so except the initial allocation to reserve memory ( mmap, alloc, malloc ...) the design was *lock-free*, and this arena_allocator was in the `lock_free` `namespace`, but unfortunately `benchmarking` the *lock-free* implementation the results was between **2x** and **3x** slower than `new` and `delete`, so having an arena_allocator with such bad performances was unuseful, and I didn't see any practical application for that. After different tries to optimize usage of `fences` and to reduce the number of `exchanges` I ended moving the arena allocator inside a new `namespace` named `core` and using a `mutex` to synchronize access.

By the way with this experience I discover that `mutex` as implemented by GCC (gcc version 12.0.1) are performing better than other synchronization mechanism, and I did some concurrent test up to 32 threads. Same check performed on Clang (clang version 14.0.0) are still better than lock-free, but less performing than gcc, so as soon I will have time for sure I will check the code produced by both in order to understand this difference. This is out of the scope for the arena_allocator, but I liked to share since for many person can be an in important information.

