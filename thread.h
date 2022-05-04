// This lib is an API for multi-thread programming

/* Create a new thread, starting with execution of START-ROUTINE
   getting passed ARG. 
   The new handle is stored in *NEWTHREAD. */
int thread_creator(void (*fn) (void *), void *arg);
