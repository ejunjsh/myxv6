#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

static void pmpinit();

// entry.S 需要给每个cpu设置一个栈
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// 为每个cpu在机器模式下的定时中断分配一个暂存区域
uint64 timer_scratch[NCPU][5];

// 这个是机器模式下的定时中断处理函数，定义在kernelvec.S
extern void timervec();

// 从entry.S跳到这里，函数用的栈为stack0，处在机器模式下
void
start()
{
  // 设置M之前的特权模式为管理员模式，这样当mret的时候，就可以返回管理员模式了
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // 设置M的程序计数器为main，这样mret的时候就会跳到main了
  // 需要 gcc -mcmodel=medany
  w_mepc((uint64)main);

  // 现在先禁用分页
  w_satp(0);

  // 代理所有中断和异常到管理员模式
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // 初始化时钟中断
  timerinit();

  // 设置每个cpu的hartid到tp寄存器，方便后面cpuid()函数调用
  int id = r_mhartid();
  w_tp(id);

  // allow access to all physical memory by S mode
  pmpinit();

  // 转换到管理员模式，并跳到main()
  asm volatile("mret");
}

// configures the pmp registers trivially so we can boot. it is not permitted
// to jump to S mode without having these configured as instruction fetch will
// fail, however we do not actually use them for protection in xv6, so we just
// need to put something trivial there.
//
// see section 3.6.1 "Physical Memory Protection CSRs" in the RISC-V privileged
// specification (v20190608)
//
// "If no PMP entry matches an M-mode access, the access succeeds. If no PMP
// entry matches an S-mode or U-mode access, but at least one PMP entry is
// implemented, the access fails." (3.6.1)
static void
pmpinit()
{
  // see figure 3.27 "PMP address register format, RV64" and table 3.10 "NAPOT
  // range encoding in PMP address and configuration registers" in the RISC-V
  // privileged specification
  // we set the bits such that this matches any 56-bit physical address
  w_pmpaddr0((~0ULL) >> 10);
  // then we allow the access
  w_pmpcfg0(PMP_R | PMP_W | PMP_X | PMP_MATCH_NAPOT);
}

// 设置机器模式下cpu能收到定时中断
// 这样中断到来时会跳到kernelvec.S下的timervec
// timervec会把这个中断转换成软件中断
// 然后跳到trap.c的devintr()函数
void
timerinit()
{
  // 每个cpu都有它自己的定时中断
  int id = r_mhartid();

  // 请求CLINT给一个定时中断
  int interval = 1000000; // 周期；在qemu里，大概1/10秒
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // 为timervec准备信息在scratch[]
  // scratch[0..2] : 用来给timervec保存寄存器
  // scratch[3] : CLINT MTIMECMP 寄存器地址
  // scratch[4] : 保存定时中断之间的间隔（周期数）
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  // 设置机器模式下的陷阱(trap)处理函数
  w_mtvec((uint64)timervec);

  // 启用机器模式下的所有中断
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // 启用机器模式下的定时中断
  w_mie(r_mie() | MIE_MTIE);
}
