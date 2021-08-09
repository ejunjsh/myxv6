// 物理内存布局

// qemu -machine virt 是这样设置的,
// 基于qemu的 hw/riscv/virt.c:
//
// 00001000 -- 引导 ROM, 由qemu提供
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- 引导 ROM 在机器模式下跳到这里
//             -kernel 在这里加载内核
// 80000000之后为未使用的RAM.

// 内核使用物理内存分布:
// 80000000 -- entry.S, 内核的代码和数据
// end -- 内核页分配区域开始的地方
// PHYSTOP -- 内核可用内存结束的地方

// qemu 放 UART 寄存器 到下面的内存地址.
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio 接口
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

#define E1000_IRQ 33

// 核心本地中断器（CLINT），它包含计时器。
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // 自引导开始后的周期数.

// qemu把平台级中断控制器（PLIC）放在这里。
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// 内核期望这里开始的内存将用来给内核和用户分配页
// 从物理地址 0x80000000 到 PHYSTOP
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// 将trampoline页映射到用户和内核空间中的最高地址。
#define TRAMPOLINE (MAXVA - PGSIZE)

// //映射trampoline下面的内核堆栈，每个堆栈都被无效的保护页包围。
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// 用户内存布局。
// 地址从0开始:
//   代码
//   原始数据和bss
//   固定大小的栈
//   可扩展堆
//   ...
//   TRAPFRAME (p->trapframe, trampoline使用)
//   TRAMPOLINE (与内核中的页相同)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
