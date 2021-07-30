// 互斥锁。
struct spinlock {
  uint locked;       // 锁被锁住了吗?

  // For debugging:
  char *name;        // 锁的名称.
  struct cpu *cpu;   // 持有锁的cpu.
  #ifdef LAB_LOCK
  int nts;
  int n;
#endif
};

