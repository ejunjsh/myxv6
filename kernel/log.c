#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// 允许并发FS系统调用的简单日志记录。
//
// 日志事务包含多个FS系统调用的更新。
// 日志系统仅在没有活动的FS系统调用时提交。
// 因此，对于提交是否会将未提交的系统调用的更新写入磁盘，不需要任何推理。
//
// 系统调用应该调用begin_op()/end_op()来标记它的开始和结束。
// 通常begin_op()只是增加正在进行的FS系统调用和返回的计数。
// 但如果它认为日志快用完了
// 一直休眠到最后一个未完成的end_op()提交。
//
// 日志是包含磁盘块的物理重做(re-do)日志。
// 磁盘日志格式:
//   头块, 包含一个块号数组，给 block A, B, C, ...
//   块 A
//   块 B
//   块 C
//   ...
// 日志添加是同步的。

// 头块的内容，用于磁盘上的头块
// 并在提交前在内存中跟踪记录的块。
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // 有多少FS系统调用正在执行
  int committing;  // 在调用commit()中, 请等待.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// 将提交的块从日志复制到其在磁盘真正的位置
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // 读日志块
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // 读磁盘目标的块
    memmove(dbuf->data, lbuf->data, BSIZE);  //  拷贝块到目标的块
    bwrite(dbuf);  // 写目标块到磁盘
    if(recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// 将日志头从磁盘读入内存中的日志头
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// 将内存中的日志头写入磁盘。
// 这是当前事务提交的真实点。
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // 如果已提交，则从日志复制到磁盘
  log.lh.n = 0;
  write_head(); // 清除日志
}

// 在每次FS系统调用开始时调用。
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // 此op可能会耗尽日志空间；等待提交。
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// 在每个FS系统调用结束时调用。
// 如果这是最后一次未完成的操作，则提交。
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op()可能正在等待日志空间，
    // 递减log.outstanding会减少保留空间的数量。
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // 由于不允许带锁睡眠，因此不带锁的调用提交。
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// 将修改的块从缓存复制到日志。
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // 日志块
    struct buf *from = bread(log.dev, log.lh.block[tail]); // 缓存块
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // 写日志
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // 将修改的块从缓存写入日志
    write_head();    // 将头块写入磁盘--真正的提交
    install_trans(0); // 现在把写操作的块写回到磁盘真正的位置
    log.lh.n = 0;
    write_head();    // 从日志中删除事务
  }
}

// 调用者已经修改了b->data，并用缓冲区完成了。
// 记录块号和增加缓存的引用计数来防止缓存被回收
// commit()/write_log()将执行磁盘写入。
//
// log_write() 替换 bwrite(); 一种典型的使用:
//   bp = bread(...)
//   修改 bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion (这个怎么翻译？ 日志吸收？)
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {  // 向日志添加新块?
    bpin(b);
    log.lh.n++;
  }
  release(&log.lock);
}

