// 这是哪个hart（核心）？
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// 机器状态寄存器（Machine Status Register），mstatus

#define MSTATUS_MPP_MASK (3L << 11) // 之前的模式
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)    // 机器模式中断启用（machine-mode interrupt enable）.

// 读状态
static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

// 写状态
static inline void 
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

// 机器异常程序计数器（machine exception program counter），
// 保存从异常返回的指令地址。
static inline void 
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// 内核状态寄存器（Supervisor Status Register），sstatus

#define SSTATUS_SPP (1L << 8)  // 之前的模式, 1=内核, 0=用户
#define SSTATUS_SPIE (1L << 5) // 内核之前的中断启用（Supervisor Previous Interrupt Enable）
#define SSTATUS_UPIE (1L << 4) // 用户之前的中断启用（User Previous Interrupt Enable）
#define SSTATUS_SIE (1L << 1)  // 内核中断启用（Supervisor Interrupt Enable）
#define SSTATUS_UIE (1L << 0)  // 用户中断启用（User Interrupt Enable）

// 读状态
static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

// 写状态
static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}
 
// 内核中断挂起（Supervisor Interrupt Pending）
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void 
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}

// 内核中断启用（Supervisor Interrupt Enable）
#define SIE_SEIE (1L << 9) // 外部
#define SIE_STIE (1L << 5) // 定时
#define SIE_SSIE (1L << 1) // 软件
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void 
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}

// 机器模式中断启用（Machine-mode Interrupt Enable）
#define MIE_MEIE (1L << 11) // 外部
#define MIE_MTIE (1L << 7)  // 定时
#define MIE_MSIE (1L << 3)  // 软件
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

static inline void 
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}

// 内核异常程序计数器（Supervisor exception program counter），
// 保存从异常返回的指令地址。
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

// 机器异常代理（Machine Exception Delegation）
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

static inline void 
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}

// 机器中断代理（Machine Interrupt Delegation）
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

static inline void 
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

//内核陷阱矢量基地址（Supervisor Trap-Vector Base Address）
//低两位是模式。？
static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

// 机器模式中断向量（Machine-mode interrupt vector）
static inline void 
w_mtvec(uint64 x)
{
  asm volatile("csrw mtvec, %0" : : "r" (x));
}

// 使用riscv的sv39页表方案。
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// 内核地址翻译和保护；（supervisor address translation and protection）
// 保存页表的地址。
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

// 内核抓痕寄存器（Supervisor Scratch register）
// 主要用在早起trampoline.S里的陷阱处理
static inline void 
w_sscratch(uint64 x)
{
  asm volatile("csrw sscratch, %0" : : "r" (x));
}

static inline void 
w_mscratch(uint64 x)
{
  asm volatile("csrw mscratch, %0" : : "r" (x));
}

// 内核陷阱发生原因（Supervisor Trap Cause）
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// 内核陷阱值（Supervisor Trap Value）
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

// 机器模式计数器启用（Machine-mode Counter-Enable）
static inline void 
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

// 机器模式周期计数器（machine-mode cycle counter）
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}

// 启用设备中断
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// 禁用设备中断
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// 设备中断启用了吗？
static inline int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// 读写tp，线程指针，它保存
// 这个核心的hartid（核心号），cpu[]的索引。
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

// 读取函数返回地址
static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}

// 刷新 TLB.
static inline void
sfence_vma()
{
  // zero，zero表示刷新所有TLB条目。
  asm volatile("sfence.vma zero, zero");
}


#define PGSIZE 4096 // 每一个页的字节数
#define PGSHIFT 12  // 页内的偏移位

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define PTE_V (1L << 0) // 有效
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // 1 -> 用户可以访问

// 移动物理地址到一个正确地方就能转化为PTE
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

// 👆的反操作
#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 从虚拟地址中提取三个9位页表索引。
#define PXMASK          0x1FF // 9 位
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
// 最大虚拟内存地址（上面不知道怎么翻译😄）
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512个PTE
