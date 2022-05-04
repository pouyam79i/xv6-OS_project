#include <thread.h>
#include <types.h>
#include <user.h>

#define PGSIZE 4096

/* Create a new thread, starting with execution of START-ROUTINE
   getting passed ARG. 
   The new handle is stored in *NEWTHREAD. */
int thread_creator(void (*fn) (void *), void *arg)
{
    int tid = -1;                              // thread id
    int mod;
    void *nsptr = malloc(2 * PGSIZE);          // new stack pointer - 2 * PGSIZE bytes allocated
    void *stack;

    if(nsptr == 0){
        // failed to allocate space 
        return -1;
    }

    // checking for page-algined stack space
    mod = ((uint)nsptr % PGSIZE);
    if(mod == 0){
        // It is page-aligned
        stack = nsptr;
    }else{
        // making it page-aligned
        stack = nsptr + (PGSIZE - mod);
    }

    // creating thread child
    tid = thread_create((void *)stack);

    if(tid < 0){
        // thread_creating failed
        goto failed;
    }
    else if(tid == 0){
        // it is inside child thread
        (fn)(arg);
        free(stack);
        exit();
    }else{
        // it is inside parent thread
        return tid;
    }

failed:
    free(nsptr);
    return -1;
}
