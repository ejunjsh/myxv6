// 磁盘文件系统格式
// 内核和用户程序会用这个头文件

#define ROOTINO  1   // 根inode号 
#define BSIZE 1024  // 块大小

// 磁盘布局:
// [ 引导块 | 超级块 | 日志 | inode 块 | 空闲位图 | 数据块]
//
// mkfs计算超级块并构建一个初始文件系统。这个
// 超级块描述磁盘布局：
struct superblock {
  uint magic;        // 必须是FSMAGIC
  uint size;         // 文件系统映像的大小（单位是块）
  uint nblocks;      // 数据块数
  uint ninodes;      // inode数量
  uint nlog;         // 日志块的数量
  uint logstart;     // 第一个日志块的块号
  uint inodestart;   // 第一个inode块的块号
  uint bmapstart;    // 第一个空闲位图块的块号
};

#define FSMAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// 磁盘inode结构
struct dinode {
  short type;           // 文件类型
  short major;          // 主设备号（仅用在T_DEVICE类型）
  short minor;          // 次设备号（仅用在T_DEVICE类型）
  short nlink;          // 文件系统中到inode的链接数
  uint size;            // 文件大小（字节）
  uint addrs[NDIRECT+1];   // 数据块地址
};

// 每个块里的inode数量
#define IPB           (BSIZE / sizeof(struct dinode))

// 块包含inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// 每个块的位图位数
#define BPB           (BSIZE*8)

// 空闲位图块，它包含块b在位图里的位
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// 目录是一个文件包含一系列dirent结构
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

