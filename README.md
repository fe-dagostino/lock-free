# Lock-Free data structures

This repository will be populated with primarily with lock-free data structures, keeping implementation simple and hopefully readable for everyone, and with other useful data structures. 

Current data structures implementations leverage C++20, so in order to build examples or to use such the following implementations a compiler that support at least C++20 is a pre-requirements. 

---
## Lock-Free implementations

Actually, the following lock-free data structures have been implemented:
* ring-buffer 
* multi-queue

### ring-buffer  **(*not finalize*)**

This implementation leverage the std::array a well specified amount of `slots` where to hold our data.

This data structure can be shared between multiple threads in both reading and writing operations since all operation in each single `slot` are synchronized.

This structure can be used in all circumstances where we don't care about the order of execution, this means that each thread can take a different time to process the single `data` unit. 

### multi-queue  **(*not finalize*)**

I guess there are ton of implementation for lock-free queue and this is one more.
This implementation can be used as:
- ***Single Queue***: shared between a 1-producer and 1-consumer as well as N-producer and M-Consumer. Each single queue is FIFO compliant. 
- ***Multiple Queues***: useful in each scenario where we have multiple producers (threads) and we can dedicate one thread to one queue and we have one or more consumer taking data from all of this queue. 
**Note**: FIFO is guaranteed on each single queue, but not between queues.
To leverage maximum performance from this implementation, it is necessary to use preallocated `nodes`.

---
## Other implementations

* [arena_allocator](./pages/core/arena_allocator/README.md) : allocate() and deallocate() with a complexity of O(1).
* [mem_unique_ptr]()


