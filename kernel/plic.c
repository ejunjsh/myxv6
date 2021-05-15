#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

//
// riscv 平台级别中断控制器（PLIC）
//

void
plicinit(void)
{
  // 设置IRQ优先级为非零值，相当于启用中断（否则的话是禁用）
  *(uint32*)(PLIC + UART0_IRQ*4) = 1;
  *(uint32*)(PLIC + VIRTIO0_IRQ*4) = 1;
}

void
plicinithart(void)
{
  int hart = cpuid();
   
  // 为当前cpu的S-mode下设置UART和VIRTIO0位
  *(uint32*)PLIC_SENABLE(hart)= (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

  // 设置当前cpu的S-mode的优先级阀值为0
  *(uint32*)PLIC_SPRIORITY(hart) = 0;
}

// 问PLIC发到当前cpu的中断是什么
int
plic_claim(void)
{
  int hart = cpuid();
  int irq = *(uint32*)PLIC_SCLAIM(hart);
  return irq;
}

// 告诉PLIC这个中断已经处理完
void
plic_complete(int irq)
{
  int hart = cpuid();
  *(uint32*)PLIC_SCLAIM(hart) = irq;
}
