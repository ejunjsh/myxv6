//
// 控制台输入和输出，到uart。
// 每次读取一行。
// 实现特殊输入字符：
//   newline -- 行尾
//   control-h -- 退格
//   control-u -- 清空一行
//   control-d -- 文件结尾
//   control-p -- 打印进程列表
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

//
// 向uart发送一个字符。
// 由printf调用，同时也用于回显输入字符，
// 但不会被write()调用
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // 如果用户键入退格，请用空格覆盖
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // 输入缓冲区
#define INPUT_BUF 128
  char buf[INPUT_BUF];
  uint r;  // 读索引
  uint w;  // 写索引
  uint e;  // 修改索引
} cons;

//
// 用户调用write()写控制台会到这里
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uartputc(c);
  }

  return i;
}

//
// 用户调用read()读控制台会到这里
// 将整个输入行复制到dst。
// user_dist表示dst是否为用户
// 或内核地址。
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // 等待中断处理程序将一些输入放入cons缓冲区。
    while(cons.r == cons.w){
      if(myproc()->killed){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF];

    if(c == C('D')){  // 文件结束
      if(n < target){
        // 保存^D以备下次使用，以确保调用者获得0字节的结果。
        cons.r--;
      }
      break;
    }

    // 将输入字节复制到用户空间缓冲区
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // 整行已经到达，返回到用户级别read()。
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

//
// 控制台输入中断处理程序。
// uartintr()为输入字符调用此函数。
// 执行擦除/终止处理，附加到cons.buf，
// 如果一整行都到了，唤醒consoleread()。
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // 打印进程列表.
    procdump();
    break;
  case C('U'):  // 删除一行.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // 退格
  case '\x7f':
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF){
      c = (c == '\r') ? '\n' : c;

      // 回显给用户
      consputc(c);

      // 存储供consoleread()使用.
      cons.buf[cons.e++ % INPUT_BUF] = c;

      if(c == '\n' || c == C('D') || cons.e == cons.r+INPUT_BUF){
       // 如果遇到换行符（或文件结尾），则唤醒consoleread()。
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // 将读写系统调用连接到consoleread和consolewrite。
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
