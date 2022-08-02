# [Lock-Free Repository](../../../README.md)

## lock_free::arena_allocator  &  core::arena_allocator

An arena_allocator implementation that keep both `allocate()` and `deallocate()` to a complexity of O(1).
 
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

The above function are simplified, since there is no synchronization for the full implementation, please refer to [arena_allocator.h](../../../include/arena_allocator.h) or to [core/arena_allocator.h](../../../include/core/arena_allocator.h) where there are two different methods for both `allocate()` and `deallocate()`, one version is thread *safe* the other one is *unsafe*, then synchronization for *unsafe_* version is in charge to the caller. 

Let's do some `benckmarks` in different conditions. 
First results are obtained in without `multi-threads`, so without `context switch` and without `concurrency`, since we want to see the differet behaviours in different conditions. The following first table have been  obtained using [nanobench](https://nanobench.ankerl.com/) with the [bm_arena_allocator.cpp](../../../benchmarks/bm_arena_allocator.cpp). Compiler `gcc 12.0.1 20220319` CPU `11th Gen Intel(R) Core(TM) i7-11800H @ 2.30GHz`.

|               ns/op |                op/s |    err% |          ins/op |          cyc/op |    IPC |         bra/op |   miss% |     total | benchmark
|--------------------:|--------------------:|--------:|----------------:|----------------:|-------:|---------------:|--------:|----------:|:----------
|               16.92 |       59,111,448.27 |    0.2% |          233.62 |           38.92 |  6.003 |          53.62 |    0.0% |      7.63 | `Using new and delete`
|               21.60 |       46,293,002.98 |    0.0% |           53.00 |           49.74 |  1.066 |           7.00 |    0.0% |      9.74 | `Using Lock-Free arena_allocator`
|               20.77 |       48,149,902.91 |    0.1% |           46.00 |           47.81 |  0.962 |           4.00 |    0.0% |      9.41 | `Using Lock-Free arena_allocator unsafe`
|               12.40 |       80,662,221.74 |    0.9% |          186.00 |           28.53 |  6.518 |          43.00 |    0.0% |      5.67 | `Using Core arena_allocator`
|                8.61 |      116,171,334.50 |    1.4% |           36.00 |           19.81 |  1.817 |           3.00 |    0.0% |      3.88 | `Using Core arena_allocator unsafe`

The table show the difference between the classic `new` and `delete` compared with `allocate()` and `deallocate()` in the *arena_allocator* in both modes *safe* and *unsafe* modes and both implementations.

In this specific benchmark we have only one thread allocating and deallocating memory, but when your program has many other calls to new and delete with different sizes, then running the program for hours will degrade `new` and `delete` performances when the arena_allocator keep performances constant in time.
There is nothing magic since the arena allocator leverage the fact that it has all `memory_slot` with the same size, instead the `new` and `delete` should deal with generic requests and even with `tcache` optimization and/or `buddy` implementations the `new` operation will cost more than O(1) to search a new slot for the user.

Now let's move on a multi-thread environment ...

