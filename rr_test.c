#include "types.h"
#include "user.h"

#pragma GCC optimize("O0")
int main()
{
    int a = 0;
    int b = 0;
    #pragma GCC unroll 0
    for(int i = 0; i <= 1<<30; i++)
    {
        a += 1;
    }
    printf(1,"%d %d", a, b);
    exit();
}