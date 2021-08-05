#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * 内核页表.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld 会设置这个在内核代码结束那里.

extern char trampoline[]; // trampoline.S

// 为内核创建一个直接映射页表。
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart 寄存器
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio 磁盘接口
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // 映射内核可执行代码和只读的数据
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // 映射内核数据和我们将使用的物理内存
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // 将陷阱的进出用到的蹦床（trampoline）映射到内核中最高的虚拟地址。
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // 映射内核栈
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// 初始化一个内核页表
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// 将h/w页表寄存器切换到内核的页表,
// 并启用分页。
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// 返回页表pagetable中PTE的地址
// 对应于虚拟地址va。如果alloc != 0,
// 创建任何必需的页表页。
//
// risc-v Sv39方案有三级页表页
// 页表页包含512个64位PTE。
// 64位虚拟地址分为五个字段：
// 39..63 -- 必须为零。
// 30..38 —- 2级索引的9位。
// 21..29 —- 一级索引的9位。
// 12..20 —- 0级索引的9位。
//  0..11 -— 页内字节偏移量的12位。
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// 查找虚拟地址，返回物理地址，
// 如果未映射，则为0。
// 只能用于查找用户页。
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  // if((*pte & PTE_U) == 0)
  //   return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// 向内核页表添加映射。
// 仅在引导时使用。
// 不刷新TLB或启用分页。
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// 为从va开始的虚拟地址创建PTE，该地址引用从pa开始的物理地址。
// va和大小可能没有页对齐。
// 成功时返回0，如果walk()无法分配所需的页，则返回-1。
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V){
      printf("%p",*pte);
      printf("%p",PTE2PA(*pte));
      backtrace();
      panic("remap");
    }
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 删除从va开始的映射的npages页。va必须页对齐。映射必须存在。
// 可以选择释放物理内存。
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      // panic("uvmunmap: walk");
      continue;
    if((*pte & PTE_V) == 0)
      // panic("uvmunmap: not mapped");
      continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// 创建空的用户页表。
// 如果内存不足，则返回0。
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// 对于第一个进程，将用户initcode加载到pagetable的地址0中。
// sz必须小于一页。
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

//分配PTE和物理内存以将进程从oldsz增长到newsz，
//它不需要页面对齐。返回新大小或错误时返回0。
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// 取消分配用户页以将进程大小从oldsz带到newsz
// oldsz和newsz不需要页面对齐，newsz也不需要必须小于oldsz。
// oldsz可以大于实际进程大小
// 返回新进程大小。
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// 递归释放页表页。
// 必须已删除所有叶映射。
void
freewalk(pagetable_t pagetable)
{
  // 页表中有2^9=512个PTE。
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // 此PTE指向较低级别的页表。
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// 释放用户内存页，
// 然后是释放页表页。
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// 给定父进程的页表，复制
// 它的内存进入孩子的页表。
// 复制页表和
// 物理内存。
// 成功时返回0，失败时返回-1。
// 失败时释放所有分配的页。
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  // char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      // panic("uvmcopy: pte should exist");
      continue;
    if((*pte & PTE_V) == 0)
      // panic("uvmcopy: page not present");
      continue;
    pa = PTE2PA(*pte);
    *pte &= ~PTE_W;
    *pte |= PTE_C;
    flags = PTE_FLAGS(*pte);
    // if((mem = kalloc()) == 0)
    //   goto err;
    // memmove(mem, (char*)pa, PGSIZE);
    // if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
    //   kfree(mem);
    //   goto err;
    // }
    // lab cow
    if(mappages(new, i, PGSIZE, pa, flags) != 0) 
      goto err;
    kref(pa);
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// 将PTE标记为用户访问无效。
// 用在exec时，设置用户堆栈保护页。
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// 从内核复制到用户。
// 将len字节从src复制到给定页表中的虚拟地址dstva。
// 成功时返回0，错误时返回-1。
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
        if (handle_pagefault(va0, myproc()) == -1) return -1; else pa0 = walkaddr(pagetable, va0);
    } else {
      if (*(walk(pagetable, va0, 0)) & PTE_C) handle_pagefault(va0, myproc());
      pa0 = walkaddr(pagetable, va0);
    }
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// 从用户复制到内核。
// 将len字节从给定页表中的虚拟地址srcva复制到dst。
// 成功时返回0，错误时返回-1。
int
_copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// 将以null结尾的字符串从用户复制到内核。
// 将字节从给定页表中的虚拟地址srcva复制到dst，
// 直到“\0”或最大值。
// 成功时返回0，错误时返回-1。
// int
// copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
// {
//   uint64 n, va0, pa0;
//   int got_null = 0;

//   while(got_null == 0 && max > 0){
//     va0 = PGROUNDDOWN(srcva);
//     pa0 = walkaddr(pagetable, va0);
//     if(pa0 == 0)
//       return -1;
//     n = PGSIZE - (srcva - va0);
//     if(n > max)
//       n = max;

//     char *p = (char *) (pa0 + (srcva - va0));
//     while(n > 0){
//       if(*p == '\0'){
//         *dst = '\0';
//         got_null = 1;
//         break;
//       } else {
//         *dst = *p;
//       }
//       --n;
//       --max;
//       p++;
//       dst++;
//     }

//     srcva = va0 + PGSIZE;
//   }
//   if(got_null){
//     return 0;
//   } else {
//     return -1;
//   }
//} 

int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
    if(srcva >= PLIC){
      return _copyin(pagetable,dst,srcva,len);
    } else{
      return copyin_new(pagetable,dst,srcva,len);
    }
}    

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
    return copyinstr_new(pagetable,dst,srcva,max);
}

// lab page table
void printwalk(pagetable_t pt, int dep) {
    for(int i = 0; i < 512; i++){
        pte_t pte = pt[i];
        if (pte & PTE_V) {
            for (int j = 0; j < dep - 1; j++) printf(".. ");
            printf("..%d: pte %p ", i, pte);
            uint64 child = PTE2PA(pte);
            printf("pa %p\n", child);
            if ((pte & (PTE_R|PTE_W|PTE_X)) == 0)
                printwalk((pagetable_t)child, dep + 1);
        }
    }
}

void vmprint(pagetable_t pt) {
    printf("page table %p\n", pt);
    printwalk(pt, 1);
}

pagetable_t proc_kpagetable(void) {
    pagetable_t kpagetable = (pagetable_t) kalloc();
    memset(kpagetable, 0, PGSIZE);

    if (mappages(kpagetable, UART0, PGSIZE, UART0, PTE_R | PTE_W) != 0) return 0;
    if (mappages(kpagetable, VIRTIO0, PGSIZE, VIRTIO0, PTE_R | PTE_W) != 0) return 0;
    if (mappages(kpagetable, PLIC, 0x400000, PLIC, PTE_R | PTE_W) != 0) return 0;
    if (mappages(kpagetable, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R | PTE_X) != 0) return 0;
    if (mappages(kpagetable, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R | PTE_W) != 0) return 0;
    if (mappages(kpagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X) != 0) return 0;

    return kpagetable;
}
int kvmcopy(pagetable_t old, pagetable_t new, uint64 st, uint64 en) {
    pte_t *pte;
    uint64 pa, i;
    uint flags;

    if (en >= PLIC) return -1;

    st = PGROUNDUP(st);

    for(i = st; i < en; i += PGSIZE) {
        if((pte = walk(old, i, 0)) == 0)
            // panic("kvmcopy: pte should exist");
            continue;
        if((*pte & PTE_V) == 0)
            // panic("kvmcopy: page not present");
            continue;
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte) & (~PTE_U);
        if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0) goto err;
    }
    return 0;
err:
    uvmunmap(new, 0, i / PGSIZE, 0);
    return -1;
}
void proc_kfreepagetable(pagetable_t pagetable) {
    for(int i = 0; i < 512; i++){
        pte_t pte = pagetable[i];
        if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
            uint64 child = PTE2PA(pte);
            proc_kfreepagetable((pagetable_t)child);
            pagetable[i] = 0;
        }
    }
    kfree((void*)pagetable);
}
uint64 kvmdealloc(pagetable_t kpagetable, uint64 oldsz, uint64 newsz) {
    if(newsz >= oldsz)
        return oldsz;

    if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        uvmunmap(kpagetable, PGROUNDUP(newsz), npages, 0);
    }
    return newsz;
}

int mmapcopy(pagetable_t old, pagetable_t new, uint64 sz) {
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 1L << 37; i < (1L << 37) + sz; i += PGSIZE) {
    if((pte = walk(old, i, 0)) == 0) continue;
    if((*pte & PTE_V) == 0) continue;
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, i, (i - (1L << 37)) >> PGSHIFT, 1);
  return -1;
}