/*
All the main functions with respect to the MeMS are inplemented here
read the function discription for more details
REFER DOCUMENTATION FOR MORE DETAILS ON FUNCTIONS AND THEIR FUNCTIONALITY
*/

#include <stdio.h>
#include <stdlib.h>

// added other headers as required

#include <sys/mman.h>

/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this
macro to make the output of all system same and conduct a fair evaluation.
*/
#define PAGE_SIZE 4096

// added other macros as required

#define MEMS_VADDR_START 1000 // Starting virtual addreass
#define HOLE 0
#define PROCESS 1
#define UNMAP_FAILED -1

// Structure for Sub chain doubly linked list node

typedef struct Sub_Node
{
    int type;         // 0-holes, 1-process
    void *vaddr_base; // start of virutal address of a sub_node
    size_t size;      // size of memory segment(bytes)
    struct Sub_Node *prev;
    struct Sub_Node *next;
} Sub_Node;

// Structure for main chain doubly linked list node

typedef struct Main_Node
{
    int pages;        // No of pages  // size of main_node(bytes) = int pages * PAGE_SIZE
    void *paddr_base; // mems_physical_addr // virtual adderes given by mmap
    void *vaddr_base; // = sub_chain_head->vaddr_base //  start of virutal address for main_node
    struct Sub_Node *sub_chain_head;
    struct Main_Node *prev;
    struct Main_Node *next;
} Main_Node;

const size_t SUB_NODE_SIZE = sizeof(Sub_Node);
const size_t MAIN_NODE_SIZE = sizeof(Main_Node);

// free list memory

Main_Node *free_list; // free_list = main_chain_head
Main_Node *main_chain_tail;
// void *free_list_page_address;  // address of page where free list is present
void *free_list_memory_offset;        // address(inside free list page) free to allocate a node (Main / Sub) in free list
size_t free_list_page_size_available; // size available in current page of free list memory

/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/
void allocate_new_free_list_page() // new page for nodes if previous page is full // called when size available < size required
{
    free_list_memory_offset = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // offset is new page
    if (free_list_memory_offset == MAP_FAILED)
    {
        perror("map free list memory\n");
        exit(1);
    }
    free_list_page_size_available = PAGE_SIZE; // new page is allocated
}

void mems_init()
{
    free_list = NULL;
    main_chain_tail = NULL;
    free_list_memory_offset = NULL;
    free_list_page_size_available = 0;
}

/*
This function will be called at the end of the MeMS system and its main job is to unmap the
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish()
{
    Main_Node *cur_main_node = free_list;
    while (cur_main_node != NULL)
    {
        size_t cur_main_size = (cur_main_node->pages) * PAGE_SIZE;
        int unmap = munmap(cur_main_node->paddr_base, cur_main_size);
        if (unmap == UNMAP_FAILED)
        {
            perror("unmap user memory");
            exit(1);
        }
        cur_main_node = cur_main_node->next;
    }
    // munmap(free_list_memory_offset, free_list_size);
}

/*
Allocates memory of the specified size by reusing a segment from the free list if
a sufficiently large segment is available.

Else, uses the mmap system call to allocate more memory on the heap and updates
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/

Sub_Node *find_hole(size_t size_req) // return NULL if size not found
{
    Main_Node *cur_main_node = free_list;
    while (cur_main_node != NULL)
    {
        Sub_Node *cur_sub_node = cur_main_node->sub_chain_head;
        while (cur_sub_node != NULL)
        {
            if ((cur_sub_node->type == HOLE) && (cur_sub_node->size >= size_req))
            {
                return cur_sub_node;
            }
            cur_sub_node = cur_sub_node->next;
        }
        cur_main_node = cur_main_node->next;
    }
    return NULL;
}

void *allocate_process(Sub_Node *hole, size_t size) // (size) will be allocated to (hole) to become process and leave remaining hole
{
    if (hole->size == size)
    {
        hole->type = PROCESS;
        // return (void *)hole->vaddr_base;
    }
    else // (hole->size > size)
    {
        if (free_list_page_size_available < SUB_NODE_SIZE)
        {
            allocate_new_free_list_page();
        }
        Sub_Node *remaining_hole = (Sub_Node *)free_list_memory_offset; // creating a new hole node for unallocated segment
        free_list_memory_offset = (char *)free_list_memory_offset + SUB_NODE_SIZE;
        // (char*)page casts offset to a char*, which treats it as a byte pointer.
        free_list_page_size_available = free_list_page_size_available - SUB_NODE_SIZE;

        remaining_hole->prev = hole;
        remaining_hole->next = hole->next;
        remaining_hole->type = HOLE;
        remaining_hole->vaddr_base = (hole->vaddr_base) + size;
        remaining_hole->size = (hole->size) - size;

        hole->next = remaining_hole;
        if (remaining_hole->next != NULL)
        {
            remaining_hole->next->prev = remaining_hole; // addon
        }

        hole->type = PROCESS;
        hole->size = size;

        // return (void *)hole->vaddr_base;
    }

    return hole->vaddr_base;
}

void *mems_malloc(size_t size)
{
    // Edge cases
    if (size <= 0)
    {
        return NULL;
    }

    // Main code
    Sub_Node *free_hole = find_hole(size);
    if (free_hole != NULL) // free hole founded // make free_hole node process node
    {
        return allocate_process(free_hole, size);
    }
    else // (free_hole == NULL) //  free hole not founded // add new main node
    {

        int total_pages = (size / PAGE_SIZE) + ((size % PAGE_SIZE) != 0); // total pages to allocated for new main node (used ceil to calculate)

        if (free_list_page_size_available < MAIN_NODE_SIZE)
        {
            allocate_new_free_list_page();
        }

        // allocating new main node
        Main_Node *new_main_node = (Main_Node *)free_list_memory_offset;
        free_list_memory_offset = (char *)free_list_memory_offset + MAIN_NODE_SIZE;
        // (char*)page casts offset to a char*, which treats it as a byte pointer.
        free_list_page_size_available = free_list_page_size_available - MAIN_NODE_SIZE;
        //

        new_main_node->pages = total_pages;
        new_main_node->next = NULL;

        if (free_list == NULL) // if empty free list
        {
            new_main_node->prev = NULL;
            new_main_node->vaddr_base = (void *)MEMS_VADDR_START;
            free_list = new_main_node; // new main node = main chain head
        }
        else
        {
            new_main_node->prev = main_chain_tail;
            new_main_node->vaddr_base = (main_chain_tail->vaddr_base) + (main_chain_tail->pages * PAGE_SIZE);
            main_chain_tail->next = new_main_node;
        }

        if (free_list_page_size_available < SUB_NODE_SIZE)
        {
            allocate_new_free_list_page();
        }

        // allocating new head hole when  new main node is formed
        Sub_Node *new_head_hole = (Sub_Node *)free_list_memory_offset;
        free_list_memory_offset = (char *)free_list_memory_offset + SUB_NODE_SIZE;
        // (char*)page casts offset to a char*, which treats it as a byte pointer.
        free_list_page_size_available = free_list_page_size_available - SUB_NODE_SIZE;
        //

        new_head_hole->prev = NULL;
        new_head_hole->next = NULL;
        new_head_hole->type = HOLE;
        new_head_hole->size = total_pages * PAGE_SIZE;
        new_head_hole->vaddr_base = new_main_node->vaddr_base;

        new_main_node->sub_chain_head = new_head_hole;

        //  allocating memory for user
        void *user_memory = mmap(NULL, total_pages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (user_memory == MAP_FAILED)
        {
            perror("map user memory");
            exit(1);
        }
        new_main_node->paddr_base = user_memory;
        //

        main_chain_tail = new_main_node;

        return allocate_process(new_main_node->sub_chain_head, size);
    }
}

/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_sub_node(Sub_Node *sub_node)
{
    if (sub_node == NULL)
    {
        printf("NULL");
        return;
    }
    (sub_node->type == HOLE) ? printf("H") : printf("P");
    void *sub_node_vaddr_bound = sub_node->vaddr_base + sub_node->size - 1;
    // void* for my 64 bit system is represented in 8 bytes which size is equal to unsigned long
    printf("[%lu:%lu]", (unsigned long)sub_node->vaddr_base, (unsigned long)sub_node_vaddr_bound);
    return;
}

void mems_print_main_node(Main_Node *main_node)
{
    void *main_node_vaddr_bound = main_node->vaddr_base + (main_node->pages * PAGE_SIZE) - 1;
    printf("MAIN[%lu:%lu]-> ", (unsigned long)main_node->vaddr_base, (unsigned long)main_node_vaddr_bound);
    Sub_Node *cur_sub_node = main_node->sub_chain_head;
    while (cur_sub_node != NULL)
    {
        mems_print_sub_node(cur_sub_node);
        printf(" <-> ");
        cur_sub_node = cur_sub_node->next;
    }
    mems_print_sub_node(cur_sub_node);
    return;
}

void print_array(int a[], int len)
{
    printf("[");
    for (int i = 0; i < len; i++)
    {
        printf("%d, ", a[i]);
    }
    printf("]\n");
}

void mems_print_stats()
{
    int main_chain_length = 0;
    int pages_used = 0;
    size_t space_unused = 0;
    printf("------- MeMs SYSTEM STATS -------\n");
    Main_Node *cur_main_node = free_list;
    while (cur_main_node != NULL)
    {
        mems_print_main_node(cur_main_node);
        printf("\n");
        main_chain_length++;
        cur_main_node = cur_main_node->next;
    }

    int sub_chain_length_array[main_chain_length];
    cur_main_node = free_list;
    int i = 0;
    while (cur_main_node != NULL)
    {
        pages_used += cur_main_node->pages;
        Sub_Node *cur_sub_node = cur_main_node->sub_chain_head;
        int sub_chain_length = 0;
        while (cur_sub_node != NULL)
        {
            if (cur_sub_node->type == HOLE)
            {
                space_unused += cur_sub_node->size;
            }
            sub_chain_length++;
            cur_sub_node = cur_sub_node->next;
        }
        sub_chain_length_array[i] = sub_chain_length;
        i++;
        cur_main_node = cur_main_node->next;
    }
    printf("Pages used : %d\n", pages_used);
    printf("Space unused : %lu\n", space_unused);
    printf("Main Chain Length : %d\n", main_chain_length);
    printf("Sub Chain Length array : ");
    print_array(sub_chain_length_array, main_chain_length);
    printf("---------------------------------\n");
}

/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void *mems_get(void *v_ptr)
{
    Main_Node *cur_main_node = free_list;
    while (cur_main_node != NULL)
    {
        size_t main_node_vgap = v_ptr - (cur_main_node->vaddr_base);
        size_t cur_main_size = PAGE_SIZE * cur_main_node->pages;
        if (main_node_vgap < cur_main_size) // main node for given virtual address is found
        {
            return cur_main_node->paddr_base + main_node_vgap;
        }
        cur_main_node = cur_main_node->next;
    }
    fprintf(stderr, "segementation fault\n");
    exit(1);
}
/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: nothing
*/

Sub_Node *find_process_segement(void *v_ptr) // find process node whose vaddr_base = v_ptr
{
    Main_Node *cur_main_node = free_list;
    while (cur_main_node != NULL)
    {
        size_t main_node_vgap = v_ptr - (cur_main_node->vaddr_base);
        size_t cur_main_size = PAGE_SIZE * cur_main_node->pages;
        if (main_node_vgap < cur_main_size) // main node for given virtual address is found
        {
            break;
        }
        cur_main_node = cur_main_node->next;
    }
    if (cur_main_node == NULL)
    {
        fprintf(stderr, "mems_free(): invalid virtual pointer\nAborted (core dumped)\n");
        exit(1);
    }
    Sub_Node *cur_sub_node = cur_main_node->sub_chain_head;
    while (cur_sub_node != NULL)
    {
        if ((cur_sub_node->vaddr_base == v_ptr) && (cur_sub_node->type == PROCESS))
        {
            return cur_sub_node;
        }
        cur_sub_node = cur_sub_node->next;
    }
    // error // process segment with vaddr_base == v_ptr not found
    fprintf(stderr, "mems_free(): invalid virtual pointer\nAborted (core dumped)\n");
    exit(1);
}

void combine_nexthole(Sub_Node *hole_node) // handling fragmentation // if input subnode is hole and next subnode is also node it will combine them
{
    Sub_Node *next_hole_node = hole_node->next;
    // error case
    if (hole_node->type == PROCESS) // never gonna reach
    {
        fprintf(stderr, "node is process\n");
        return;
    }
    // Edge Cases
    if (next_hole_node == NULL)
        return;
    if (next_hole_node->type == PROCESS)
        return;
    // combine  // like deleting next hole node from sub chain
    hole_node->next = next_hole_node->next;
    if (next_hole_node->next != NULL)
    {
        next_hole_node->next->prev = hole_node;
    }
    hole_node->size += next_hole_node->size;
    return;
}

void mems_free(void *v_ptr) // v_ptr can only be  pointer values which were returned by mems_malloc
{
    Sub_Node *process_node = find_process_segement(v_ptr); // process segment corresponding to v_ptr
    process_node->type = HOLE;                             // process node become hole node
    combine_nexthole(process_node);
    if (process_node->prev != NULL)
    {
        if (process_node->prev->type == HOLE)
            combine_nexthole(process_node->prev);
    }
    return;
}
