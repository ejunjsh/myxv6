#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// 帮助唤醒那些正在wait()的父进程，保证他们不会错失唤醒机会
// 帮组在使用p->parent时，遵从内存模型
// 他必须在p->lock之前获取
struct spinlock wait_lock;


// 为每个进程的内核堆栈分配一个页。
// 将它映射到内存的高位，后跟一个无效的
// 保护页。
void
proc_mapstacks(pagetable_t kpgtbl) {
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// 在引导时初始化proc表。
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->kstack = KSTACK((int) (p - proc));
  }
}

// 必须在禁用中断的情况下调用，
// 防止进程移动到其他CPU时发生争用。
int
cpuid()
{
  int id = r_tp();
  return id;
}

// 返回此CPU的CPU结构。
// 必须禁用中断。
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

//返回当前struct proc*，如果没有，则返回零。
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

// 分配pid
int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// 在进程表中查找未使用（UNUSED）的进程。
// 如果找到，初始化在内核中运行所需的状态，
// 返回时要持有p->lock锁。
// 如果没有空闲进程，或者内存分配失败，则返回0。
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // 分配一个trapframe页
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 一个空的用户页表
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 设置新的上下文开始在forkret执行，它返回用户空间。
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// 释放进程结构和挂在它身上的数据
// 包括用户页表
// 必须持有p->lock才能调用这个函数 
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// 为给定进程创建用户页表，
// 没有用户内存，但是有trampoline页。
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // 一个空的页表
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // 映射trampoline代码（用于系统调用返回）在最高的用户虚拟地址。
  // 内核模式下会使用它，在返回用户模式和从用户模式来的时候，所以不能加上PTE_U标志
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // 为trampoline.S 把trapframe映射到TRAMPOLINE的下方。
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// 释放进程的页表，然后释放
// 它所指的物理内存。
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// 一个用户程序，它调用了exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// 设置第一个用户进程。
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // 分配一个用户页并将init的指令和数据复制到其中。
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // 准备从内核到用户的第一次“返回”。
  p->trapframe->epc = 0;      // 用户程序计数器
  p->trapframe->sp = PGSIZE;  // 用户栈指针

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// 将用户内存增加或减少n字节。
// 成功时返回0，失败时返回-1。
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// 创建一个新进程，复制父进程。
// 将子内核堆栈设置为从fork()系统调用返回。
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // 分配进程
  if((np = allocproc()) == 0){
    return -1;
  }

  // 从父进程拷贝用户内存到子进程
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // 拷贝保存到用户寄存器
  *(np->trapframe) = *(p->trapframe);

  // 令fork在子进程返回0
  np->trapframe->a0 = 0;

  // 增加打开文件描述符的引用数
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// 把p的孩子交给init进程
// 调用者必须持有wait_lock
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// 退出当前进程。不会返回。
// 退出的进程仍处于僵尸状态
// 直到其父亲进程调用wait()。
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // 关闭所有打开的文件
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // 把子进程交给init照顾
  reparent(p);

  // 父进程有可能在wait()上睡眠了
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // 跳进调度器，不再返回
  sched();
  panic("zombie exit");
}

// 等待子进程退出并返回其pid。
// 返回 -1，如果此进程没有子进程。
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // 在表中搜索退出的子进程
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // 确保子进程不在exit()或者swtch()上
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // 发现一个
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // 如果我们没有任何子进程，等也没用
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // 等待一个子进程退出
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// 每个cpu的进程调度器
// 每个cpu在设置好自己后，都会调用scheduler()
// 调度器不会返回。它会一直循环，做下面的事：
// - 选择一个进程去运行
// - 切换并开始运行那个进程
// - 最后,那个进程会切换回到调度器并交出控制权
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // 通过确保设备中断是打开的来避免死锁
    intr_on();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // 切换到选择到进程。
        // 释放进程锁，在跳回到这里之前还要重新获取锁，这是进程的工作
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // 进程目前已完成运行。
        // 它应该在回来之前改变它的p->state。
        c->proc = 0;

        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0){
      intr_on();
      wfi(); // 中断来之前，停住cpu
    }
  }
}

// 切换到调度器。 必须持有 p->lock 和 已经修改了proc->state
// 保存和恢复intena， 因为intena是一个内核线程的属性，不是CPU的
// 这应该是进程的属性的，例如proc->intena 和 proc->noff
// 但这样做会在一些地方有问题，例如一个锁被持有了，但是没有进程
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// 为一轮调度放弃CPU。
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// fork的子进程的第一次调度到这里，由调度器scheduler()调度
// 将切换到forkret
void
forkret(void)
{
  static int first = 1;

  // p->lock仍然被调度器持有，所以这里要释放
  release(&myproc()->lock);

  if (first) {
    // 文件系统初始化必须在一个常规的进程上下文里
    //（例如，因为它调用了sleep），因此不能
    // 从main()运行。
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// 原子释放锁和睡眠在chan上。
// 唤醒时重新获取锁定。
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // 为了改变p->state和之后调用sched，必须获得p->lock,
  // 一旦我们获得p->lock,我们就能保证不会错过任何的唤醒
  // （唤醒锁 p->lock） 
  // 这样可以释放lk了

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // 休眠
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // 清理
  p->chan = 0;

  // 重新获取原来的锁
  release(&p->lock);
  acquire(lk);
}

// 唤醒休眠在chan上的所有进程
// 必须在调用之前不要持有任何p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}


// 给定一个pid来杀死一个进程
// 这个进程不会马上退出直到它尝试回到用户空间（可以看看trap.c的usertrap()方法）
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // 唤醒sleep()的进程
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// 复制到用户地址或内核地址，
// 取决于usr_dst。
// 成功时返回0，错误时返回-1。
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// 从用户地址或内核地址复制，
// 取决于usr_src。
// 成功时返回0，错误时返回-1。
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// 将进程列表打印到控制台。用于调试。
// 当用户在控制台上键入^P时运行。
// 没有锁，以避免进一步楔入卡住的机器。
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
