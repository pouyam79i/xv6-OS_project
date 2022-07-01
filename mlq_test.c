#include "types.h"
#include "stat.h"
#include "user.h"

#pragma GCC optimize("O0")
int main()
{
    
    int is_child = 0;
    int child_num = 0;
    uint priority = 0;
    change_policy(3);           // seting scheduler policy to num 3 (multi leyered queued scheduler)
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
        get_proc_timing();
        exit();
    }
    else
    {
        set_priority(11);
        for (int i = 0; i < 60; i++)
            wait();
    }
    change_policy(0);           // return to defult scheduler
    exit();
}
