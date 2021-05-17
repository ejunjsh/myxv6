//
// 16550a UART的低级驱动程序例程.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// UART控制寄存器在地址UART0处进行内存映射。
// 此宏返回其中一个寄存器的地址。
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

// UART控制寄存器。有些对读和写有不同的含义.
// 详情看 http://byterunner.com/16550.html
#define RHR 0                 // 接收保持寄存器（用于输入字节）
#define THR 0                 // 传输保持寄存器（用于输出字节）
#define IER 1                 // 中断启用寄存器
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // 先进先出(FIFO)控制寄存器
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // 清除两个FIFO的内容
#define ISR 2                 // 中断状态寄存器
#define LCR 3                 // 线路控制寄存器
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // 设置波特率的特殊模式
#define LSR 5                 // 线路状态寄存器
#define LSR_RX_READY (1<<0)   // RHR可以读取数据了
#define LSR_TX_IDLE (1<<5)    // THR可以接受另一个字符发送

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// 发送（传输，输出）缓冲区.
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w; // 写入uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]时加1
uint64 uart_tx_r; // 读出uart_tx_buf[uar_tx_r % UART_TX_BUF_SIZE]时加1

extern volatile int panicked; // 来自 printf.c

void uartstart();

void
uartinit(void)
{
  // 禁用中断
  WriteReg(IER, 0x00);

  // 设置波特率的特殊模式。
  WriteReg(LCR, LCR_BAUD_LATCH);

  // 波特率为38.4K的LSB。
  WriteReg(0, 0x03);

  // 波特率为38.4K时为MSB。
  WriteReg(1, 0x00);

  // 离开设置波特率模式，并设置字长度为8位，没有奇偶校验。
  WriteReg(LCR, LCR_EIGHT_BITS);

  // 重设和启用FIFO
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // 启用发送和接收中断
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

  initlock(&uart_tx_lock, "uart");
}

// 加一个字符到输出缓冲区同时告诉UART开始发送，如果它还没有
// 如果输出缓冲区满了，则阻塞
// 由于可能阻塞，所以这个函数不能在中断处理里面调用
// 只适合用在write()系统调用里面
void
uartputc(int c)
{
  acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }

  while(1){
    if(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){
      // 缓冲区满
      // 等待uartstart()去腾出空间
      sleep(&uart_tx_r, &uart_tx_lock);
    } else {
      uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
      uart_tx_w += 1;
      uartstart();
      release(&uart_tx_lock);
      return;
    }
  }
}

// uartputc()的备选版本，这个版本没有用中断
// 用在内核的printf()函数来回应字符
// 它自循等待uart输出寄存器为空
void
uartputc_sync(int c)
{
  push_off();

  if(panicked){
    for(;;)
      ;
  }

  // 等待UART传输寄存器为空
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);

  pop_off();
}

// 如果UART空闲，同时传输缓冲区有字符，发送它
// 调用者必须持有uart_tx_lock锁
// 这个函数被上半部和下半部调用
void
uartstart()
{
  while(1){
    if(uart_tx_w == uart_tx_r){
      // 传输的缓冲区是空的，表示没有数据要发送，直接返回
      return;
    }
    
    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){
      // UART 传输寄存器满了，没办法接受其他字节了
      // 所以如果UART可以接受新的数据的时候，会再次发起中断的
      return;
    }
    
    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;
    
    // 唤醒那些调用uartputc()时被阻塞的，正在等待缓冲区腾出空间的进程
    wakeup(&uart_tx_r);
    
    WriteReg(THR, c);
  }
}

// 从UART读取一个字符
// 如果读未就绪则返回-1
int
uartgetc(void)
{
  if(ReadReg(LSR) & 0x01){
    // 输入数据准备好了
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

// 处理uart中断，有可能是输入进来了，有可能是可以输出了，也可能两样都可以了
// trap.c调用这个函数
void
uartintr(void)
{
  // 读取和处理进来的字符
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // 发送缓存区里的字符
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
