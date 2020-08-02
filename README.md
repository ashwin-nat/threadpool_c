# Simple Threadpool in C

This is a simple implementation of a threadpool in C. Yes, there are several of these on github, but this is my take on a simple solution of this problem.

This code has been tested on linux (it doesn't work in OSX since unnamed semaphores aren't supported).

This implementation uses semaphores for synchronisation as opposed to the mutex and conditional variable pair.

This threadpool is simple to include in any project of yours, just copy the source file and include the header file.

The threading library used here is the POSIX thread (pthreads) and queues are maintained using doubly linked lists.

There are just 3 functions in this library
* `tpool_create()` - Creates a threadpool instance and returns a pointer to the thread pool handle
* `tpool_add_job()` - Enqueues a job onto the given threadpool. An optional destructor can be provided for cleanup of the given argument
* `tpool_destroy()` - Waits for all worker threads to terminate and then destroys the given threadpool. If there are jobs remaining on the queue, the threadpool can be configured to perform the jobs before destroying the pool.



## Getting Started

To build the example code, use GNU make to build using the provided makefile.
```
make
```
