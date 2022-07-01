#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct ticket {
  struct proc *proc;
  uint tickets;
};

unsigned int g_seed = 0;

// Used to seed the generator.           
void fast_srand(int seed) {
    g_seed = seed;
}

// Compute a pseudorandom integer.
// Output value in range [0, 32767]
int rand(void) {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

/*
* Scheduler Type
* 0: Round Robin with quantum = 1 tick
* 1: Round Robin with quantum = QUANTUM tick (set in param.h)
* 2: Priority Scheduling (6 levels)
* 3: Multi Level Queue
* 4: Lottery Scheduling
*/
int schedtype = 1;
int
change_policy(int new_policy){
  if(new_policy < 0 && new_policy > 4){
    return -1;
  }
  acquire(&ptable.lock);
  schedtype = new_policy;
  release(&ptable.lock);
  return 0;
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->ctime = ticks;   //setting creation time
  p->tstack = -1;     //initialize stack top
  p->tcount = 1;      //initialize thread count
  p->bticks = 0;      //initialize burst ticks
  p->priority = 3;    //initialize with moderate priority
  p->tt = 0;          //initialize termination time
  p->ru_t = 0;        //initialize running time
  p->re_t = 0;        //initialize ready time
  p->st = 0;          //initialize sleeping time

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  np->priority = curproc->priority;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{

  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->tt = ticks;        // setting termination time
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(schedtype == 0 || schedtype == 1) // Round Robin
      {
        if(p->state != RUNNABLE)
            continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        // Process comes back to scheduler from here
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      else if (schedtype == 2) // Priority
      {
        struct proc *lowest = ptable.proc;
        struct proc *p1;
        //find process with lowest priority
        for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
        {
          if (p1->state != RUNNABLE)
            continue;
          else if (lowest->state != RUNNABLE)
            lowest = p1;
          else if (lowest->priority > p1->priority)
            lowest = p1;
        }
        //switch to lowest
        c->proc = lowest;
        switchuvm(lowest);
        lowest->state = RUNNING;
        swtch(&(c->scheduler), lowest->context);
        switchkvm();
        c->proc = 0;
      }
      else if (schedtype == 3) // MFQ
      {
        // find the highest priority process
        int lowest = 6;
        struct proc *p1;
        //find runnable process with lowest priority
        for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
        {
          if (p1->state != RUNNABLE)
            continue;
          if (p1->priority < lowest)
            lowest = p1->priority;
        }
        // do a loop on all processes round robin style, scheduling that priority queue
        for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
        {
          if(schedtype != 3)
            break;
          if(p->state != RUNNABLE || p->priority != lowest)
            continue;
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        // Process comes back to scheduler from here
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        }
      }
      else if (schedtype == 4) // Lottery Scheduling
      {
        struct ticket active_tickets[NPROC];
        uint ticket_count = 0;
        uint active_count = 0;
        struct proc *p1;
        // find total tickets of active processes
        // each process has 'priority' tickets
        for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
        {
          if (p1->state != RUNNABLE)
            continue;
          active_tickets[active_count].proc = p1;
          active_tickets[active_count].tickets = p1->priority;
          active_count += 1;
          ticket_count += p1->priority;
        }
        // main scheduler loop
        while(ticket_count > 0)
        {
          int random = rand() % ticket_count;
          //cprintf("%d", random);
          //find which process the ticket belongs to
          int i;
          int passed = 0; //number of tickets that we've "passed" in the loop
          for(i = 0; i < active_count; i++)
          {
            if((passed + active_tickets[i].tickets) >= random)
            {
              //found
              break;
            }
            else
            {
              passed += active_tickets[i].tickets;
            }
          }
          struct proc *p_selected = active_tickets[i].proc;
          //cprintf("\n%d %d %s\n", p_selected->kstack, p_selected->pid, p_selected->name);
          // remove a ticket from selected process
          ticket_count -= 1;
          active_tickets[i].tickets -= 1;
          // run selected processs
          c->proc = p_selected;
          switchuvm(p_selected);
          p_selected->state = RUNNING;
          swtch(&(c->scheduler), p_selected->context);
          switchkvm();
          c->proc = 0;
        }
      }
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// Beginning of my changes: 
/*
  I am going to add my addition system calls  below here.
  Author: Pouya Mohammadi (9829039)
*/

// This syscall returns number of done clocks
// from when os is loaded until now!
int
getTicks(void){
  // Priniting total passed clocks
  cprintf("Total Clocks: ");
  acquire(&tickslock);
  cprintf("%d", ticks);
  release(&tickslock);
  cprintf("\n");
  return 0;
}

// This syscall will show processes and
// will show run-time of running processes.
int 
getProcInfo(void){
  cprintf("PID -> CTIME\n");
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNING){
      cprintf("%d -> %d\n", p->pid, p->ctime);
    }else{
      cprintf("Not running process. PID: %d\n", p->pid);
    }
  }
  return 0;
}

// End of my changes.

// Phase 2 System Calls:

// Creates a new process like fork, but doesn't copy the process' memory
// Only sets the stack pointer to new stack location
int
thread_create(void *stack){
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  np->tstack = (int)((char *)stack + PGSIZE);
  np->tcount = -1; //np is a thread so -1
  curproc->tcount++; //add one thread to curproc thread count

  acquire(&ptable.lock);
  np->pgdir = curproc->pgdir;
  np->sz = curproc->sz;
  release(&ptable.lock);
  *np->tf = *curproc->tf; //this goddamn line.
  int stack_size = curproc->tstack - curproc->tf->esp;
  // set thread stack poitner to bottom of stack
  np->tf->esp = np->tstack - stack_size;
  // copy parent stack to child thread
  memmove((void *)np->tf->esp, (void *)curproc->tf->esp, stack_size);
  // same for thread base pointer
  np->tf->ebp = np->tstack - (curproc->tstack - curproc->tf->ebp);
  np->parent = curproc;
  //the aforementioned goddamned line used to be here and completely ruin everything i did to esp and ebp.
  // Clear %eax so that create_thread returns 0 in the child thread.
  np->tf->eax = 0;

  //copy file descriptors
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  pid = np->pid;
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  return pid;
}

int
thread_id(void){
  struct proc *curproc = myproc();
  if (curproc->tcount != -1) //if not thread
    return -1;
  return curproc->pid;
}

int
thread_join(uint tid){
  struct proc *p;
  int tid_found = 0;
  struct proc *curproc = myproc();
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if (p->pid == tid){
      tid_found = 1;
      break;
    }
  }
  release(&ptable.lock);
  if (!tid_found){
    return -1;
  }
  if (p->tcount != -1) //if isn't thread
    return -1; //don't wait
  acquire(&ptable.lock);
  for(;;){
    if(p->state == ZOMBIE){
      // Found zombie child thread with same pid == tid
      kfree(p->kstack);
      p->kstack = 0;
      //removed a freeuvm function here as we don't remove the page table for threads
      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      p->pgdir = 0;
      p->state = UNUSED;
      release(&ptable.lock);
      return 0;
    }
    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Used to set or change priority of processes in scheduler
int
set_priority(uint priority){
  struct proc *curproc = myproc();
  acquire(&ptable.lock);
  curproc->priority = priority;
  release(&ptable.lock);
  if(curproc->priority != priority)
    return -1;
  return priority;
}

// updates timing of processes
// warning: remember to lock ticks befor calling this syscall
// warning: ptable must be unlock befor this syscall
int
update_proc_timing(void){
  struct proc *p;
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNABLE){
      p->re_t++;    // increasing ready time
    }else if (p->state == RUNNING)
    {
      p->ru_t++;    // increasing running time
    }else if (p->state == SLEEPING)
    {
      p->st++;      // increasing sleeping time
    }
  }
  release(&ptable.lock);
  return 0;
}

// prints timing info of pid if found
int 
get_proc_timing(void){
  struct proc *curproc;
  curproc = myproc();
  acquire(&ptable.lock);
  // cprintf("For PID %d: {ct: %d, tt: %d, re_t: %d, ru_t: %d, st: %d}\n", p->pid, p->ctime, p->re_t, p->ru_t, p->st);
  int waiting_time = curproc->st + curproc->re_t;
  int turnaround_time = waiting_time + curproc->ru_t;
  cprintf("For PID %d: {\ncreation time: %d,\ntermination time: %d,\nturnaround time: %d,\nburst time: %d,\nwaiting time: %d\n}\n", curproc->pid, curproc->ctime, curproc->tt, turnaround_time, curproc->ru_t, waiting_time);
  release(&ptable.lock);
  return 0;
}
