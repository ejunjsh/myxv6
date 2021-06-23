// 创建一个僵尸进程，它的父进程退出时肯定帮它换了父亲

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  if(fork() > 0)
    sleep(5);  // 让子进程比父进程退出早
  exit(0);
}
