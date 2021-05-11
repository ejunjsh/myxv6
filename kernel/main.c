#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// 所有cpu从start() 跳到这里，这时候就是内核模式了 
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // 物理页分配器
    kvminit();       // 创建内核页表
    kvminithart();   // 启用分页
    procinit();      // 进程表
    trapinit();      // 陷阱向量
    trapinithart();  // 安装内核陷阱向量
    plicinit();      // 设置中断控制器
    plicinithart();  // 向PLIC配置设备中断
    binit();         // 缓冲区缓存
    iinit();         // inode缓存
    fileinit();      // 文件表
    virtio_disk_init(); // 模拟硬盘
    userinit();      // 第一个用户进程
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // 启用分页
    trapinithart();   // 安装内核陷阱向量
    plicinithart();   // 向PLIC配置设备中断
  }

  scheduler();        
}
