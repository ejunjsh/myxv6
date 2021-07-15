// 为内核上下文切换时保存的寄存器。
struct context {
  uint64 ra;
  uint64 sp;

  // 被调用保存
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// 每个CPU的状态.
struct cpu {
  struct proc *proc;          // 在此cpu上运行的进程，或为空。
  struct context context;     // swtch() 这里再进入 scheduler().
  int noff;                   // push_off() 嵌套的深度.
  int intena;                 // 在push_off()之前是否启用了中断？
};

extern struct cpu cpus[NCPU];

// 每个进程都会有的，用来保存在trampoline.S中陷阱处理代码的数据。位于用户页表中的trampoline页下面的页中。
// 在内核页表中没有特别映射。
// sscratch 寄存器指向这里
// trampoline.S中的uservec将用户寄存器保存在trapframe中，然后从trapframe的kernel_sp、kernel_hartid、kernel_satp初始化寄存器，并跳转到kernel_trap。
// usertrapret()设置了trapframe的kernel_*字段， 在trampoline.S 的userret 从trapframe 恢复用户寄存器， 切换用户页表，进入用户空间
// trapframe 包含了被调用保存的用户寄存器，例如s0-s11，因为通过usertrapret()返回用户路径不会通过整个内核调用堆栈返回。
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表
  /*   8 */ uint64 kernel_sp;     // 进程的内核栈顶
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // 保存用户程序计数器
  /*  32 */ uint64 kernel_hartid; // 保存内核tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 每个进程状态
struct proc {
  struct spinlock lock;

  // 使用这些必须持有p->lock:
  enum procstate state;        // 进程状态
  void *chan;                  // 如果非零，则进程进入睡眠状态，chan则用来作为唤醒的。
  int killed;                  // 如果非零，则进程被杀死了
  int xstate;                  // 退出状态，用来返回给父进程的wait
  int pid;                     // 进程id

  // 使用这个必须持用wait_lock:
  struct proc *parent;         // 父进程

  // 这些是进程私有的，因此不需要持有p->lock。
  uint64 kstack;               // 内核栈的虚拟地址
  char *kstackpa;              // 内核栈的物理地址
  uint64 sz;                   // 进程内存大小（字节）
  pagetable_t pagetable;       // 用户页表
  pagetable_t kpagetable;      // 用户内核页表
  struct trapframe *trapframe; // trampoline.S的数据页
  struct context context;      // swtch() 到这里然后运行进程
  struct file *ofile[NOFILE];  // 打开的文件
  struct inode *cwd;           // 当前目录
  char name[16];               // 进程名字(调试用)
  int tracemask;               // 实验（syscall）加的，用来跟踪系统调用
};
