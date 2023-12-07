# OS_CS238P_Projects
UC Irvine CS398P Operating Systems Projects

Project 1:
  - Write C program into C file, spawn new process using fork and execv to compile this file
  - Compile the file as dynamically loadable library (.so)
  - Using dlopen, load the compiled .so file
  - Using dlsym, find the C program and execute the program


Project 2:
  - Dynamic scheduling of userspace threads
  - Thread invokes yield
  - Using setjmp we store the state of execution for the current thread
  - We move onto next thread and use longjmp to restore the state of execution of that thread
  - Controling the yield from every thread is impossible, so this program registers signal handler for SIGALRM which is sent to program periodically by calling alarm


Project 3:
  - Use file on disk as a backbone and implement our own malloc function
  - Any call to malloc should provide an address and the specified size should be available for write
  - The data is written to the file
  - This is achieved using mmap, and we manage the memory by adding metadata for every allocated memory chunk
  - free() frees up the memory, and that memory is resusable
  - The data is persisted, and upon re-execution of the program, we can access the same data


Project 4:
  - Use block device (loopback device) to implement a KV store
  - Implemented byte addressable read/writes as a wrapper around block addressable read/writes (pread/pwrite)
  - The wrapper module takes care of caching, to avoid write amplification
  - Write/Read chaches are maintained to make writes as async as possible and to avoid latency of pwrite/pread
  - Worked thread is spanwed to write the data from write cache to device
  - Beautiful solution for producer consumer problem in project-4/logfs.c. consumer - write_to_device() and producer - write_to_queue()


Project 5:
  - Implement 'top' like system monitoring service
  - Use of /proc filesystem to get all the data and dynamically print all the data
