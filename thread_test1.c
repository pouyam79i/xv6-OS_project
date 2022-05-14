#include "types.h"
#include "user.h"
#include "thread.h"


void route_t(void *);
void question(void);

int limit = 4;
int base = 0;

int main(void)
{
    printf(2, "Base: %d, Limit: %d\n", base, limit);
    uint tid = thread_creator(&route_t, 0);
    if (tid > 0){
        thread_joiner(tid);
    }else{
        printf(0, "ID:[Parent] => Failed to cread chiled thread\n");
    }
    exit();
}

void route_t(void *arg)
{
    question();
    return;
}

void question(void){
    base++;
    if(base < limit){
        printf(1, "ID:[%d] => [SUCCESS] 0\n", get_tid());
        uint tid = thread_creator(&route_t, 0);
        if(tid > 0){
            thread_joiner(tid);
        }else{
            printf(1, "ID:[%d] => Failed to cread another thread\n", get_tid());
        }
    }else{
        printf(1, "ID:[%d] => [FAILED] -1\n", get_tid());
    }
    return;
}
