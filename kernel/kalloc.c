// 物理内存分配器，给用户进程，内核栈，页表页和管道缓存分配物理内存
// 每次分配都是4096字节的页
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // 内核后面的第一个地址
                   // 由kernel.ld定义

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

// 页的引用计数，lab cow
struct {
    struct spinlock lock;
    uint a[32768]; 
} refcnt;

void
kinit()
{
  for (int i = 0; i < NCPU; i++) 
    initlock(&kmem[i].lock, "kmem");
  initlock(&refcnt.lock, "refcnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// 释放物理内存页，一般pa即kalloc的返回值
// （初始化这个分配器的时候，会有异常，详情可以看上面的kinit）
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if ((uint64)pa >= KERNBASE) {
      acquire(&refcnt.lock);
      if (refcnt.a[((uint64)pa - KERNBASE) >> PGSHIFT] > 1) {
          refcnt.a[((uint64)pa - KERNBASE) >> PGSHIFT]--;
          release(&refcnt.lock); return;
      } else {
          refcnt.a[((uint64)pa - KERNBASE) >> PGSHIFT] = 0;
          release(&refcnt.lock);
      }
  }

  // 用垃圾数据填充来避免悬挂引用
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  int cpu_id; push_off(); cpu_id = cpuid(); pop_off();
  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// 分配一个4096字节的物理内存页，返回一个指针给内核
// 返回0代表没办法分配内存了
void *
kalloc(void)
{
  struct run *r;

  int cpu_id; push_off(); cpu_id = cpuid(); pop_off();
  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r) {
      kmem[cpu_id].freelist = r->next;
      release(&kmem[cpu_id].lock);
  } else {
      release(&kmem[cpu_id].lock);
      for (int i = 0; i < NCPU; i++) if (i != cpu_id) {
          acquire(&kmem[i].lock);
          r = kmem[i].freelist;
          if (r) {
              kmem[i].freelist = r->next;
              release(&kmem[i].lock);
              break;
          } else release(&kmem[i].lock);
      }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // 填充垃圾数据
  kref((uint64)r);
  return (void*)r;
}

// 返回空闲内存的字节数（实验 systemcall）
uint64 nfree()
{
  struct run* r;
  uint64 cnt = 0;
  for (int i = 0; i < NCPU; i++){
    acquire(&kmem[i].lock);
    r = kmem[i].freelist;
    while (r)
    {
      cnt++;
      r = r->next;
    }
    release(&kmem[i].lock);
  }
  return cnt * PGSIZE;
}

// 引用计数
void kref(uint64 pa) {
  if (pa >= KERNBASE) {
      acquire(&refcnt.lock);
      refcnt.a[(pa - KERNBASE) >> PGSHIFT]++;
      release(&refcnt.lock);
  }
}