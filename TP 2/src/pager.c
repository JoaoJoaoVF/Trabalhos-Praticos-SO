/* UNIVERSIDADE FEDERAL DE MINAS GERAIS     *
 * DEPARTAMENTO DE CIENCIA DA COMPUTACAO    *
 * Copyright (c) Italo Fernando Scota Cunha */


#include <sys/types.h>
#include <pthread.h>


struct page_t{

}; 

struct page_list{
    int size; 
}; 

struct frames_t{
    int pid; 
};

struct frame_list{
    int size; 
    struct frames_t *frames; 
}; 

struct blocks_t{
}; 

struct block_list{
    int size; 
    struct blocks_t *blocks; 
}; 

struct table_list{
    struct page_list* pages_list; 
}; 


struct pages_list pager; 
struct frame_list frame_list; 
struct block_list block_list; 

pthread_mutex_t mutex; 

/* `pager_init` is called by the memory management infrastructure to
 * initialize the pager.  `nframes` and `nblocks` are the number of
 * physical memory frames available and the number of blocks for
 * backing store, respectively. */
void pager_init(int nframes, int nblocks){
    pthread_mutex_lock(&mutex); 
    
    frame_list.size = nframes; 
    block_list.size = nblocks; 

    frame_list.frames = malloc(nframes * sizeof(struct frames_t)); 
    block_list.blocks = malloc(nblocks * sizeof(struct blocks_t)); 


    


    pthread_mutex_unlock(&mutex); 

}

/* `pager_create` should initialize any resources the pager needs to
 * manage memory for a new process `pid`. */
void pager_create(pid_t pid){

}

/* `pager_extend` allocates a new page of memory to process `pid`
 * and returns a pointer to that memory in the process's address
 * space.  `pager_extend` need not zero memory or install mappings
 * in the infrastructure until the application actually accesses the
 * page (which will trigger a call to `pager_fault`).
 * `pager_extend` should return NULL is there are no disk blocks to
 * use as backing storage. */
void *pager_extend(pid_t pid){
}

/* `pager_fault` is called when process `pid` receives
 * a segmentation fault at address `addr`.  `pager_fault` is only
 * called for addresses previously returned with `pager_extend`.  If
 * free memory frames exist, `pager_fault` should use the
 * lowest-numbered frame to service the page fault.  If no free
 * memory frames exist, `pager_fault` should use the second-chance
 * (also known as clock) algorithm to choose which frame to page to
 * disk.  Your second-chance algorithm should treat read and write
 * accesses the same (i.e., do not prioritize either).  As the
 * memory management infrastructure does not maintain page access
 * and writing information, your pager must track this information
 * to implement the second-chance algorithm. */
void pager_fault(pid_t pid, void *addr){

}

/* `pager_syslog prints a message made of `len` bytes following
 * `addr` in the address space of process `pid`.  `pager_syslog`
 * should behave as if making read accesses to the process's memory
 * (zeroing and swapping in pages from disk if necessary).  If the
 * processes tries to syslog a memory region it has not allocated,
 * then `pager_syslog` should return -1 and set errno to EINVAL; if
 * the syslog succeeds, it should return 0. */
int pager_syslog(pid_t pid, void *addr, size_t len){

}

/* `pager_destroy` is called when the process is already dead.  It
 * should free all resources process `pid` allocated (memory frames
 * and disk blocks).  `pager_destroy` should not call any of the MMU
 * functions. */
void pager_destroy(pid_t pid){

}

