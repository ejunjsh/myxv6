struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // 引用计数器
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE 和 FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// inode的内存副本
struct inode {
  uint dev;           // 设备号
  uint inum;          // Inode号
  int ref;            // 引用计数
  struct sleeplock lock; // 保护下面的一切
  int valid;          // inode已从磁盘读取？

  short type;         // 磁盘inode副本
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2];
  char target[MAXTARGET];
};

// 将主设备编号映射到设备函数上。
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
#define STATS 2
