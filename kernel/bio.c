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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // 所有缓冲区的链表，通过head.next/head.prev。
  // 按最近使用缓冲区的时间排序。
  // head.next是最近的，head.prev是最少的。
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // 创建缓冲区的链表
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// 通过缓冲区缓存查找设备dev上的块。
// 如果找不到，则分配一个缓冲区。
// 在这两种情况下，返回锁定的缓冲区。
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // 块是否已缓存？
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 未缓存。
  // 回收最近最少使用的（LRU）未使用的缓冲区。
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
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

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 没有人在等它。
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


