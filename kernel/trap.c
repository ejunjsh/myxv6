#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// 在kernelvec.S调用kerneltrap()
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// 设置内核模式下的中断处理函数为kernelvec
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

// 在用户模式下处理中断，异常或系统调用
// 由trampoline.S调用
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 配置这时候的中断处理函数为kerneltrap(),
  // 因为现在就在内核.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // 保存用户程序计数
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // 系统调用

    if(p->killed)
      exit(-1);

    // sepc指向的ecall指令
    // 但是我们想要返回到下一个指令
    p->trapframe->epc += 4;

    // 中断会改变sstatus和寄存器的值
    // 所以在处理好这些寄存器之后才打开中断
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    if (r_scause() == 13 || r_scause() == 15) {
          uint64 va = r_stval(); 
          if (handle_pagefault(va, p) ==  -1) 
            p->killed = 1;
      } else {
          printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
          printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
          p->killed = 1;
      }
  }

  if(p->killed)
    exit(-1);

  // 如果是一个定时中断，则让出CPU
  // Lab traps
    if(which_dev == 2) {
      if (p->ticks > 0 && p->duration > -1) {
          p->duration++;
          if (p->duration >= p->ticks) {
              p->duration = -1;
              p->state_time = *p->trapframe;
              p->trapframe->epc = p->handler;
              intr_on();
          } else yield();
      } else yield();
  }

  usertrapret();
}

//
// 返回用户空间
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  // 禁用中断（不知道怎么翻译）
  intr_off();

  // 把系统调用，中断和异常都送到trampoline.S处理
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // 设置trapframe的值方便进程下次进入内核时uservec能用到
  p->trapframe->kernel_satp = r_satp();         // 内核页表
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // 进程内核栈
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid 方便cpuid()使用

  // 设置寄存器好让 trampoline.S 的sret顺利回到用户空间
  
  // 设置SSP（S Previous Privilege）为用户模式
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 清空SPP为0，这样sret时就是返回用户模式了
  x |= SSTATUS_SPIE; // 启用用户模式下的中断
  w_sstatus(x);

  // 把保存的用户pc设置回sepc（S Exception Program Counter）
  w_sepc(p->trapframe->epc);

  // 告诉trampoline.S 要切换的用户页表
  uint64 satp = MAKE_SATP(p->pagetable);

  // 跳回 trampoline.S，那里会切换会用户页表，恢复用户寄存器，和通过sret返回到用户模式
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// 发生在内核的中断和异常通过kernelvec来到这里
// 无论当前的内核栈在哪里
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) != 0){
    //ok
  }else {
    struct proc *p = myproc();
    if (r_scause() == 13 || r_scause() == 15) {
          uint64 va = r_stval(); 
          if (handle_kpagefault(va, p) == -1) {
            p->killed = 1;
            exit(-1);
          }
      } else {
          printf("scause %p\n", scause);
          printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
          panic("kerneltrap");
      }
  }

  // 如果是定时中断，则让出CPU
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // yield()可能造成很多中断发生
  // 所以这里恢复下一些寄存器，方便跳回到kernelvec.S时，对sepc指令的使用
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// 检查是否是外部中断还是软件中断，并处理它
// 如果是定时中断，返回2
// 如果是其他设备发来的中断，返回1
// 如果是未知，则返回0
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // 这是一个通过PLIC传来的外部中断

    // irq 表示哪个设备发起的中断.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // PLIC 允许每个设备一次最多发起一次中断；
    // 告诉PLIC这个设备现在可以再次发起中断来
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // 软件中断，来自于机器模式下的定时中断，从kernelvec.S的timervec转发过来的
    if(cpuid() == 0){
      clockintr();
    }
    
    // 通过清空sip下的SSIP位来回应软件中断
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

