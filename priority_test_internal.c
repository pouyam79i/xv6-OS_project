#include "types.h"
#include "user.h"

#pragma GCC optimize("O0")
int main()
{
    int pid = fork();
    if(pid == 0) //child
    {
        set_priority(2);
        for(int i = 0; i <= 1000; i++)
        {
            printf(1,"PID: %d: %d --- ",getpid(), i);
        }
        printf(1,"\n");
        exit();
    }
    else
    {
        for(int i = 0; i <= 1000; i++)
        {
            printf(1,"%d ", i);
        }
        printf(1,"\n");
        wait();
        exit();
    }
}