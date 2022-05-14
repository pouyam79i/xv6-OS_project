#include "thread.h"
#include "types.h"
#include "user.h"

#define PGSIZE 4096

/* Create a new thread, starting with execution of START-ROUTINE
   getting passed ARG. 
   The new handle is stored in *NEWTHREAD. */
int 
thread_creator(void (*fn) (void *), void *arg)
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
    mod = (uint)nsptr % PGSIZE;
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
        free(nsptr);
        exit();
    }else{
        // it is inside parent thread
        return tid;
    }

failed:
    free(nsptr);
    return -1;
}


// waits for thread id to be done
int 
thread_joiner(int tid)
{
    int res = thread_join(tid);
    return res;
}

// returns thread ID
int 
get_tid(void)
{
    int id = thread_id();
    return id;
}

// lock resources if you need them
// you have to wait untill they are relaesed!
void thread_mutex_lock(mutex_t * mutex){
    while(mutex->lock == 1);
    mutex->lock = 1;
}

// Unlock resources
void thread_mutex_unlock(mutex_t * mutex){
    mutex->lock = 0;
}
