// 互斥自旋锁.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

#ifdef LAB_LOCK
#define NLOCK 500

static struct spinlock *locks[NLOCK];
struct spinlock lock_locks;

void
freelock(struct spinlock *lk)
{
  acquire(&lock_locks);
  int i;
  for (i = 0; i < NLOCK; i++) {
    if(locks[i] == lk) {
      locks[i] = 0;
      break;
    }
  }
  release(&lock_locks);
}

static void
findslot(struct spinlock *lk) {
  acquire(&lock_locks);
  int i;
  for (i = 0; i < NLOCK; i++) {
    if(locks[i] == 0) {
      locks[i] = lk;
      release(&lock_locks);
      return;
    }
  }
  panic("findslot");
}
#endif

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;

  #ifdef LAB_LOCK
  lk->nts = 0;
  lk->n = 0;
  findslot(lk);
  #endif 
}

// 获取锁。
// 循环（旋转）直到获得锁。
void
acquire(struct spinlock *lk)
{
  push_off(); // 禁用中断以避免死锁。
  if(holding(lk))
    panic("acquire");

  #ifdef LAB_LOCK
  __sync_fetch_and_add(&(lk->n), 1);
  #endif   

  // 在RISC-V上, sync_lock_test_and_set 转变为原子交换:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0) {
  #ifdef LAB_LOCK
      __sync_fetch_and_add(&(lk->nts), 1);
  #else
    ;
  #endif
  }
  // 告诉C编译器和处理器不要移动加载或存储
  // 过了这一点，要保证关键段的内存
  // 引用严格地发生在获取锁之后。
  // 在RISC-V上，它发出一个fence指令。
  __sync_synchronize();

  // 为holding()记录锁获取信息和方便调试的
  lk->cpu = mycpu();
}

// 释放锁
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // 告诉C编译器和处理器不要移动加载或存储
  // 过了这一点，要保证关键段的所有存储在释放锁之前都是对其他CPU可见的，
  // 同时发生在关键段的加载必须严格发生在锁释放之前
  // 在RISC-V上，它发出一个fence指令。
  __sync_synchronize();

  // 释放锁，相当于lk->locked=0。
  // 这段代码不使用C赋值，因为C标准意味着一个赋值可以用多个存储指令实现
  // On RISC-V, sync_lock_release 转变为原子交换:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// 检查这个cpu是否持有锁.
// 调用这个函数，中断必须先关闭.
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off 与 intr_off()/intr_on() 类似，只是它们是匹配的：
// 需要两个ppop_off()才能撤消两个push_off()的操作。另外，如果中断
// 一开始中断是关闭的，然后push_off, pop_off会继续让中断关闭

void
push_off(void)
{
  int old = intr_get();

  intr_off();
  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}


#ifdef LAB_LOCK
int
snprint_lock(char *buf, int sz, struct spinlock *lk)
{
  int n = 0;
  if(lk->n > 0) {
    n = snprintf(buf, sz, "lock: %s: #fetch-and-add %d #acquire() %d\n",
                 lk->name, lk->nts, lk->n);
  }
  return n;
}

int
statslock(char *buf, int sz) {
  int n;
  int tot = 0;

  acquire(&lock_locks);

  n = snprintf(buf, sz, "--- lock kmem/bcache stats\n");
  for(int i = 0; i < NLOCK; i++) {
    if(locks[i] == 0)
      break;
    if(strncmp(locks[i]->name, "bcache", strlen("bcache")) == 0 ||
       strncmp(locks[i]->name, "kmem", strlen("kmem")) == 0) {
      tot += locks[i]->nts;
      n += snprint_lock(buf +n, sz-n, locks[i]);
    }
  }

  n += snprintf(buf+n, sz-n, "--- top 5 contended locks:\n");
  int last = 100000000;
  // stupid way to compute top 5 contended locks
  for(int t = 0; t < 5; t++) {
    int top = 0;
    for(int i = 0; i < NLOCK; i++) {
      if(locks[i] == 0)
        break;
      if(locks[i]->nts > locks[top]->nts && locks[i]->nts < last) {
        top = i;
      }
    }
    n += snprint_lock(buf+n, sz-n, locks[top]);
    last = locks[top]->nts;
  }
  n += snprintf(buf+n, sz-n, "tot= %d\n", tot);
  release(&lock_locks);
  return n;
}
#endif
