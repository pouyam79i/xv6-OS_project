#include "types.h"
#include "user.h"
#include "thread.h"


void t(void *);

int global = 0;

int main(void)
{
    uint tid = thread_creator(&t, 0);
    if (tid > 0){
        printf(1,"Hello from parent process %d\n",getpid());
        //thread_joiner(tid);
    }
    exit();
}

void t(void *arg)
{
    printf(1, "Hello from Thread %d\n",getpid());
    return;
}