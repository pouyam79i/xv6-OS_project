#include "types.h"
#include "user.h"

#pragma GCC optimize("O0")
int main()
{
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
        exit();
    }
    else
    {
        for (int i = 0; i < 10; i++)
            wait();
    }
}