// 进程长期锁
struct sleeplock {
  uint locked;       // 锁锁着吗？  
  struct spinlock lk; // spinlock保护此睡眠锁
  
  // 调试用
  char *name;        // 锁的名字.
  int pid;           // 持有锁的进程id
};

