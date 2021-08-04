// 缓冲区缓存(buffer cache)。
//
// 缓冲区缓存是保存磁盘块内容缓存副本的buf结构的链表。
// 在内存中缓存磁盘块可以减少磁盘读取的次数，还可以为多个进程使用的磁盘块提供同步点。
//
// 接口：
//* 要获取特定磁盘块的缓冲区，请调用bread。
//* 更改缓冲区数据后，调用bwrite将其写入磁盘。
//* 处理完缓冲区后，调用brelse。
//* 调用brelse后不要使用缓冲区。
//* 一次只能有一个进程可以使用缓冲区，
//  所以不要把它们放得太久。


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BNUM 13

struct {
  struct spinlock lock;
  struct spinlock block[BNUM];
  struct buf buf[NBUF];

  // 所有缓冲区的链表，通过head.next/head.prev。
  // 按最近使用缓冲区的时间排序。
  // head.next是最近的，head.prev是最少的。
  struct buf head[BNUM];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // 创建缓冲区的链表
  for (int i = 0; i < BNUM; i++) {
      initlock(bcache.block + i, "bcache.bucket");
      bcache.head[i].prev = &bcache.head[i];
      bcache.head[i].next = &bcache.head[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// 通过缓冲区缓存查找设备dev上的块。
// 如果找不到，则分配一个缓冲区。
// 在这两种情况下，返回锁定的缓冲区。
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int entry = blockno % BNUM;
  acquire(bcache.block + entry);

  // 块是否已缓存？
  for(b = bcache.head[entry].next; b != &bcache.head[entry]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(bcache.block + entry);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 未缓存。
  // 回收最近最少使用的（LRU）未使用的缓冲区。
  for (int i = (entry + 1) % BNUM; i != entry; i = (i + 1) % BNUM) {
      struct buf *bb = 0; uint minticks = 0x3fffffff;
      acquire(bcache.block + i);
      for(b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev)
          if (b->refcnt == 0 && b->timestamp < minticks) {
              minticks = b->timestamp; bb = b;
          }
      if (bb != 0) {
          bb->dev = dev;
          bb->blockno = blockno;
          bb->valid = 0;
          bb->refcnt = 1;
          bb->next->prev = bb->prev;
          bb->prev->next = bb->next;
          bb->next = bcache.head[entry].next;
          bb->prev = &bcache.head[entry];
          bcache.head[entry].next->prev = bb;
          bcache.head[entry].next = bb;
          release(bcache.block + i);
          release(bcache.block + entry);
          acquiresleep(&bb->lock);
          return bb;
      }
      release(bcache.block + i);
  }
  panic("bget: no buffers");
}

// 返回一个带有指示块内容的锁定buf。
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// 将b的内容写入磁盘。必须锁定。
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// 释放锁定的缓冲区。
// 移到最近使用的列表的开头。
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int entry = b->blockno % BNUM;
  acquire(bcache.block + entry);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
  }
  
  release(bcache.block + entry);
}

void
bpin(struct buf *b) {
  int entry = b->blockno % BNUM;
  acquire(bcache.block + entry);
  b->refcnt++;
  release(bcache.block + entry);
}

void
bunpin(struct buf *b) {
  int entry = b->blockno % BNUM;
  acquire(bcache.block + entry);
  b->refcnt--;
  release(bcache.block + entry);
}


