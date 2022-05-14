// This lib is an API for multi-thread programming

// initializing mutex structs
#define MUTEX_INITIALIZER(lock) {lock}

// mutex structure
typedef struct MUTEX_INITIALIZER_STRUCT{
   int lock;
}mutex_t;

/* Create a new thread, starting with execution of START-ROUTINE
   getting passed ARG. 
   The new handle is stored in *NEWTHREAD. */
int thread_creator(void (*fn) (void *), void *arg);

// waits for thread id to be done
int thread_joiner(int tid);

// returns thread ID
int get_tid(void);

// lock resources if you need them
// you have to wait untill they are relaesed!
void thread_mutex_lock(mutex_t * mutex);

// Unlock resources
void thread_mutex_unlock(mutex_t * mutex);
