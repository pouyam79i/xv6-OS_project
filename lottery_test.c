#include "types.h"
#include "stat.h"
#include "user.h"

#pragma GCC optimize("O0")
int main()
{
    
    int is_child = 0;
    int child_num = 0;
    uint priority = 0;
    change_policy(4);           // seting scheduler policy to num 4 (lottery scheduler)
    for (int i = 0; i < 60; i++)
    {
        if(i % 10 == 0){
            priority++;
        }
        child_num++;
        int pid = fork();
        if (pid == 0)
        {   
            is_child = 1;
            set_priority(priority);
            break;
        }
    }
    if (is_child)
    {
        for (int i = 1; i <= 200; i++)
        {
            printf(2, "%d : %d\n", child_num, i);
        }
        struct time_data timing;
        get_proc_timing((void*)&timing);
        int waiting_time = timing.st + timing.re_t;
        int turnaround_time = waiting_time + timing.ru_t;
        //printf(1,"Priority : %d",priority);
        printf(1,"For PID %d: {\ncreation time: %d,\ntermination time: %d,\nturnaround time: %d,\nburst time: %d,\nwaiting time: %d\n}\n", getpid(), timing.ctime, getTicks(), turnaround_time, timing.ru_t, waiting_time);
        exit();
    }
    else
    {
        for (int i = 0; i < 60; i++)
            wait();
    }
    change_policy(0);           // return to defult scheduler
    exit();
}
