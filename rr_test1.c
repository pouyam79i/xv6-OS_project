#include "types.h"
#include "user.h"

#pragma GCC optimize("O0")
int main()
{
    change_policy(1);
    int is_child = 0;
    for (int i = 0; i < 10; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            is_child = 1;
            break;
        }
    }
    if (is_child)
    {
        int pid = getpid();
        for (int i = 1; i <= 1000; i++)
        {
            printf(1, "%d : %d\n", pid, i);
        }
        struct time_data timing;
        get_proc_timing((void*)&timing);
        int waiting_time = timing.st + timing.re_t;
        int turnaround_time = waiting_time + timing.ru_t;
        printf(1,"For PID %d: {\ncreation time: %d,\ntermination time: %d,\nturnaround time: %d,\nburst time: %d,\nwaiting time: %d\n}\n", getpid(), timing.ctime, getTicks(), turnaround_time, timing.ru_t, waiting_time);
        exit();
    }
    else
    {
        for (int i = 0; i < 10; i++)
            wait();
    }
}