# Memory Management System

A custom memory management system (MeMS) using the C programming language. MeMS utilize the system calls mmap and munmap for memory allocation and deallocation, respectively.

## Table of Contents

-   [Memory Management System](#Memory-Management-System)
    -   [Constraints and Requirements](#constraints-and-Requirements)
    -   [Free List Structure](#Free-List-Structure)
    -   [MeMS Virtual Address and MeMS Physical Address](#MeMS-Virtual-Address-and-MeMS-Physical-Address)
    -   [Function Implementations](#Function-Implementations)

## Constraints and Requirements

-   MeMS solely use the system calls mmap and munmap and don't use of any other memory management library functions such as malloc, calloc, free, and realloc for memory management
-   Request memory from the OS using mmap in multiples of the system's PAGE_SIZE , for my systems its 4096 bytes (4KB)
-   Deallocate memory through munmap which occur in multiples of PAGE_SIZE.

## Free List Structure

MeMS maintains a free list data structure to keep track of the heap memory which MeMS has requested from the OS. This free list keeps track of two items:

-   **PROCESS -** Memory allocated to each user program.
-   **HOLE -** Memory which has not been allocated to any user program.

Free List is represented as a doubly linked list. Let's call this doubly linked list as the main chain of the free list. The main features of the main chain are:

1. Whenever MeMS requests memory from the OS (using mmap), it adds a new node to the main chain.
2. Each node of the main chain points to another doubly linked list which we call as sub-chain. This sub-chain can contain multiple nodes. Each node corresponds to a segment of memory within the range of the memory defined by its main chain node. Some of these nodes (segments) in the sub-chain are mapped to the user program. We call such nodes (segments) as PROCESS nodes. Rest of the nodes in the sub-chain are not mapped to the user program and are called as HOLES or HOLE nodes.

![Free_list](free_list.png)

Whenever the user program requests for memory from MeMS, MeMS first tries to find a sufficiently large segment in any sub-chain of any node in the main chain. If a sufficiently large segment is found, MeMS uses it to allocate memory to the user program and updates the segment’s type from HOLE to PROCESS. Else, MeMS requests the OS to allocate more memory on the heap (using mmap) and add a new node corresponding to it in the main chain.

The segments of type HOLE can be reallocated to any new requests by the user process. In this scenario, if some space remains after allocation then the remaining part becomes a new segment of type HOLE in that sub-chain

System avoids memory fragmentation within the free list by combining two adjacent HOLES in subchain

## MeMS Virtual Address and MeMS Physical Address

The address (memory location) returned by mmap be the MeMS physical address. In reality, the address returned by mmap is actually a virtual address in the virtual address space of the process in which MeMS is running.Since we are simulating memory management by the OS, we will call the virtual address returned by mmap as MeMS physical address.

Just like a call to mmap returns a virtual address in the virtual address space of the calling process, a call to mems_malloc will return a MeMS virtual address in the MeMS virtual address space of the calling process.MeMS manages heap memory for only one process at a time.

Just like OS maintains a mapping from virtual address space to physical address space, MeMS maintains a mapping from MeMS virtual address space to MeMS physical address space. So, for every MeMS physical address (which is provided by mmap), we need to assign a MeMS virtual address. This MeMS virtual address has no meaning outside the MeMS system.

Any time the user process wants to write/store anything to the heap, it has to make use of the MeMS virtual address. But we cannot directly write using MeMS virtual address as the OS does not have any understanding of MeMS virtual address space. Therefore, we first get the MeMS physical address for that MeMS virtual address. Then, the user process needs to use this MeMS physical address to write on the heap.

![address mapping](mapping.png)

## Function Implementations

1. void mems_init(): Initializes all the required parameters for the MeMS system. The main parameters to be initialized are
   the head of the free list i.e. the pointer that points to the head of the free list
   the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
   any other global variable that you want for the MeMS implementation can be initialized here.  
   Input Parameter: Nothing  
   Returns: Nothing

2. void mems_finish(): This function will be called at the end of the MeMS system and its main job is to unmap the allocated memory using the munmap system call.  
   Input Parameter: Nothing  
   Returns: Nothing

3. void\* mems_malloc(size_t size): Allocates memory of the specified size by reusing a segment from the free list if a sufficiently large segment is available. Else, uses the mmap system call to allocate more memory on the heap and updates the free list accordingly.  
   Parameter: The size of the memory the user program wants  
   Returns: MeMS Virtual address (that is created by MeMS)

4. void mems_free(void\* ptr): Frees the memory pointed by ptr by marking the corresponding sub-chain node in the free list as HOLE. Once a sub-chain node is marked as HOLE, it becomes available for future allocations.  
   Parameter: MeMS Virtual address (that is created by MeMS)  
   Returns: nothing

5. void mems_print_stats(): Prints the total number of mapped pages (using mmap) and the unused memory in bytes (the total size of holes in the free list). It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.  
   Parameter: Nothing  
   Returns: Nothing but should print the necessary information on STDOUT

6. void \*mems_get(void\* v_ptr): Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).  
   Parameter: MeMS Virtual address (that is created by MeMS)  
   Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).


### How to run the exmaple.c

After implementing functions in mems.h follow the below steps to run example.c file

```
$ make
$ ./example
```

### Output to example.c
![Output](example_output.jpg)
