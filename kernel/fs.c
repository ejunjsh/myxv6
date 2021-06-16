// 文件系统实现.  五层:
//   + 块（Blocks）: 原始磁盘块的分配器。
//   + 日志（Log）: 多步骤更新的崩溃恢复。
//   + 文件（Files）: inode分配器，读取，写入，元数据。
//   + 目录（Directories）: 具有特殊内容的inode（其他inode的列表！）
//   + 名字（Names）: 路径如/usr/rtm/xv6/fs.c，方便命名。
//
// 此文件包含低级文件系统操作例程。（更高级的）系统调用实现在sysfile.c中。

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// 每个磁盘设备应该有一个超级块，但我们只使用一个设备运行
struct superblock sb; 

// 读取超级块（super block）.
static void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// 初始化文件系统
void
fsinit(int dev) {
  readsb(dev, &sb);
  if(sb.magic != FSMAGIC)
    panic("invalid file system");
  initlog(dev, &sb);
}

// 清零一个块
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// 块（Blocks）.

// 分配一个清零过的磁盘块
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // 块空闲?
        bp->data[bi/8] |= m;  // 标志块被使用了.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// 释放一个磁盘块
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// inode描述单个未命名的文件。
// inode磁盘结构保存元数据：文件的类型，
// 它的大小、引用它的链接数以及
// 保存文件内容的块的列表。
//
// inode按顺序排列在sb.startinode的磁盘上。
// 每个inode都有一个数字，表示它在磁盘上的位置。
//
// 
// 内核在内存里面保留了一个正在使用中的inode表，
// 提供一个地方给多进程来同步访问inode
// 内存inode包括未存储在磁盘上的记账信息（book-keeping）：ip->ref和ip->valid。
//
// 一个inode和它的内存代表会经历一系列状态，在他们可以被剩下的文件系统代码使用之前
//
// * 分配（Allocation）： 如果他的类型(type)在磁盘是非零，那这个inode是被分配了，
//   ialloc()用来分配，iput()会在inode的引用和链接数为零时释放这个inode
//
// * 表中引用（Referencing in table）：如果ip->ref是零，则这个在inode表中条目则是空闲的
//   否则ip->ref会跟踪指向该项（打开的文件和当前目录）的内存指针的数量。
//   iget()查找或者创建一个表项和增加它的引用；iput()用来减少引用
//
// * 有效（Valid）：inode表条目中的信息(type, size, &c) 只有在ip->Valid为1时才正确。
//   ilock()从磁盘读取inode并设置ip->valid，而iput()清除ip->valid（如果ip->ref降为零）。
//
// * 锁定(Locked)：文件系统代码只能检查和修改inode中的信息及其内容，如果它首先锁定了inode。
//
// 因此，一个典型的序列是:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... 检查和修改 ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock()与iget()是分开的，因此系统调用可以获取对inode的长期引用（对于打开的文件），并且只锁定它很短的时间（例如，在read()中）。
// 这种分离还有助于避免在路径名查找过程中出现死锁和争用。iget()递增ip->ref，使inode保持在表中，指向它的指针保持有效。
//
// 许多内部文件系统函数期望调用者锁定所涉及的inode；这允许调用者创建多步骤原子操作。
//
// itable.lock自旋锁保护itable条目的分配。由于ip->ref指示一个条目是否空闲，
// 而ip->dev和ip->inum指示条目持有哪个inode，因此在使用这些字段时必须持有itable.lock。
//
// ip->lock睡眠锁保护除ref、dev和inum之外的所有ip->字段。
// 必须持有ip->lock才能读写inode的ip->valid、ip->size、ip->type,&c。

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

void
iinit()
{
  int i = 0;
  
  initlock(&itable.lock, "itable");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

static struct inode* iget(uint dev, uint inum);

// 在设备dev上分配inode。
// 通过指定类型将其标记为已分配。
// 返回一个未锁定但已分配和引用的inode。
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // 一个空闲的inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // 在磁盘上标记它为已分配
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

// 将修改的内存inode复制到磁盘。
// 必须在每次更改ip->xxx字段后调用，这些字段必须是磁盘上有的。
// 调用者必须持有ip->lock.
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// 在设备dev上找到编号为inum的inode
// 并返回内存中的副本。
// 不锁定inode，也不从磁盘读取它。
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // inode已经在表中了吗？
  empty = 0;
  for(ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // 记住空槽.
      empty = ip;
  }

  // 回收inode条目。
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&itable.lock);

  return ip;
}

// 增加ip的引用计数
// 返回ip启用ip = idup(ip1)习惯用语
struct inode*
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// 锁定给定的inode。
// 如果需要，从磁盘读取inode。
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// 解锁给定inode。
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// 删除对内存inode的引用。
// 如果这是最后一个引用，inode表条目可以被回收。
// 如果这是最后一个引用，并且inode没有指向它的链接，请在磁盘上释放inode（及其内容）。
// 所有对iput()的调用都必须在事务内部，以防它必须释放inode。
void
iput(struct inode *ip)
{
  acquire(&itable.lock);

  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // inode没有链接，也没有其他引用：删除和释放它

    // ip->ref==1表示没有其他进程可以锁定ip，
    // 所以这个acquiresleep()不会阻塞（或死锁）。
    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }

  ip->ref--;
  release(&itable.lock);
}

// 一般操作，解锁(unlock)然后放(put)
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

// Inode 内容
//
// 每个inode关联的内容（数据）都是存储在磁盘块中
// 前NDIRECT块号放在ip->addrs[]，接下来的NINDIRECT块放在块ip->addrs[NDIRECT]里面
// 返回在inode ip下第n个块的磁盘块地址
// 如果不存在这个块，bmap会分配一个
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // 加载间接块，如果需要，分配一个
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// 截断inode（丢弃内容）。
// 调用者必须持有ip->lock
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// 从inode拷贝stat信息
// 调用者必须持有ip->lock
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// 从inode读取数据
// 调用者必须持有ip->locl
// 如果user_dst==1 则dst是个用户虚拟地址
// 否则dst就是内核地址
int
readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}

// 写数据到inode
// 调用者必须持有ip->lock
// 如果user_src==1 则src是个用户虚拟地址
// 否则src就是内核地址
// 返回成功写入字节数
// 如果返回值小于请求的数量n
// 那肯定有错误发生
int
writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if(off > ip->size)
    ip->size = off;

  // 即使大小没有改变，也要将inode写回磁盘
  // 因为上面的循环可能调用了bmap()并向ip->addrs[]添加了一个新块。
  iupdate(ip);

  return tot;
}

// 目录

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// 在目录里面查找一个目录项(文件)
// 如果找到，设置*poff 为目录项的位移字节
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // 条目匹配路径元素
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// 将新目录条目（name,inum）写入目录dp。
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // 检查名字是否存在
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // 查找一个空的目录项
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

// 路径

// 从path拷贝下一个路径元素到name
// 返回指向所复制元素后面的元素的指针。
// 返回的路径没有前导斜杠，
// 因此调用者可以检查*path=='\0'以查看名称是否是最后一个。
// 如果没有要删除的名称，则返回0。
//
// 例子:
//   skipelem("a/bb/c", name) = "bb/c", 设置 name = "a"
//   skipelem("///a//bb", name) = "bb", 设置 name = "a"
//   skipelem("a", name) = "", 设置 name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// 为一个路径名查找和返回inode
// 如果parent!=0, 则返回父的inode，并拷贝最后的路径元素到name，这个name必须有空间容纳DIRSIZ字节。
// 必须在一个事务里面被调用，因为它调用了iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // 早停一层。
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
