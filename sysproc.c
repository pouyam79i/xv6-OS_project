#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// connection my process to syscalls:
// calls getTicks process
int
sys_getTicks(void){
  return getTicks();
}

// calls getProcInfo process
int 
sys_getProcInfo(void){
  return getProcInfo();
}

int
sys_thread_create(void)
{
  int stack = 0;
  if(argint(0, &stack) < 0) //to pass an integer value to a kernel level function
    return -1;
  return thread_create((void *)stack);
}

int
sys_thread_id(void)
{
  return thread_id();
}

int
sys_thread_join(void)
{
  int tid = 0;
  if(argint(0, &tid) < 0) //to pass an integer value to a kernel level function
    return -1;
  return thread_join(tid);
}

int
sys_set_priority(void){
  int priority = 0;
  if(argint(0, &priority) < 0)
    return -1;
  return set_priority(priority);
}

int 
sys_change_policy(void){
  int policy = -1;
  if(argint(0, &policy) < 0)
    return -1;
  return change_policy(policy);
}

int 
sys_update_proc_timing(void){
  return update_proc_timing();
}

int 
sys_get_proc_timing(void){
  return get_proc_timing();
}
