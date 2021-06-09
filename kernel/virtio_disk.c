//
// qemu的virtio磁盘设备的驱动程序。
// 将qemu的mmio接口用于virtio。
// qemu提供了一个“遗留”virtio接口。
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"

// virtio mmio寄存器r的地址。
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct disk {
  // virtio驱动程序和设备主要通过RAM中的一组结构进行通信。
  // pages[]分配内存。
  // pages[]是全局的（而不是调用kalloc()），
  // 因为它必须由两个连续的页对齐的物理内存页组成。
  char pages[2*PGSIZE];

  // pages[] 被划分出三个区域（描述符，可用环，已用环）
  // 如virtio遗留接口规范第2.6节所述。
  // https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
  
  // pages[]的第一个区域是一组DMA描述符（不是一个环），
  // 驱动程序用它告诉设备在哪里读写单个磁盘操作。
  // 有NUM个描述符。
  // 大多数命令都是由一些描述符组成的“链”（链表）。
  // 指向到pages[]里面
  struct virtq_desc *desc;

  // 可用环，驱动想要设备处理什么就把的描述符号写进来
  // 它只包含每个链里面的头描述符（即就是描述符链的第一个描述符）
  // 这个环有NUM个元素
  // 指向到pages[]里面
  struct virtq_avail *avail;

  // 已用环，设备会在完成处理的时候，写描述符号（只是链的头）进来
  // 有NUM个已用环条目
  // 指向到pages[]里面
  struct virtq_used *used;

  // 我们自己的记账
  char free[NUM];  // 描述符是否可用?
  uint16 used_idx; // 我们已经在used[2..NUM]中看了这么远。

  // 关于正在操作中的跟踪信息，
  // 当完成中断到达时使用。
  // 由链的第一个描述符索引索引。
  struct {
    struct buf *b;
    char status;
  } info[NUM];

  // 磁盘命令头。
  // 为了方便起见，一对一地使用描述符。
  struct virtio_blk_req ops[NUM];
  
  struct spinlock vdisk_lock;
  
} __attribute__ ((aligned (PGSIZE))) disk;

void
virtio_disk_init(void)
{
  uint32 status = 0;

  initlock(&disk.vdisk_lock, "virtio_disk");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 1 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio disk");
  }
  
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // 协商功能
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // 告诉设备功能协商已完成。
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // 告诉设备我们完全准备好了。
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

  // 初始化队列 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
  memset(disk.pages, 0, sizeof(disk.pages));
  *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> PGSHIFT;

  // desc = pages -- num * virtq_desc
  // avail = pages + 0x40 -- 2 * uint16, then num * uint16
  // used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem

  disk.desc = (struct virtq_desc *) disk.pages;
  disk.avail = (struct virtq_avail *)(disk.pages + NUM*sizeof(struct virtq_desc));
  disk.used = (struct virtq_used *) (disk.pages + PGSIZE);

  // 所有描述符开始都是未使用的。
  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // plic.c 和 trap.c 安排来自VIRTIO0_IRQ的中断
}

// 找到一个空闲描述符，将其标记为非空闲，返回其索引。
static int
alloc_desc()
{
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// 将描述符标记为可用。
static void
free_desc(int i)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(disk.free[i])
    panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
  wakeup(&disk.free[0]);
}

// 释放一链（chain）的描述符
static void
free_chain(int i)
{
  while(1){
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// 分配三个描述符（它们不必是连续的）。
// 磁盘传输总是使用三个描述符。
static int
alloc3_desc(int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

void
virtio_disk_rw(struct buf *b, int write)
{
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk.vdisk_lock);

  // 规范的第5.2节说，遗留块操作使用
  // 三个描述符：一个用于类型/保留/扇区，一个用于
  // 数据，一个表示1字节的状态结果。

  // 分配三个描述符
  int idx[3];
  while(1){
    if(alloc3_desc(idx) == 0) {
      break;
    }
    sleep(&disk.free[0], &disk.vdisk_lock);
  }

  // 格式化这三个描述符
  // qemu的virtio-blk.c 会读取他们.

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

  if(write)
    buf0->type = VIRTIO_BLK_T_OUT; // 写磁盘
  else
    buf0->type = VIRTIO_BLK_T_IN; // 读磁盘
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = (uint64) buf0;
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (uint64) b->data;
  disk.desc[idx[1]].len = BSIZE;
  if(write)
    disk.desc[idx[1]].flags = 0; // 设备读 b->data
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // 设备写 b->data
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff; // 成功时设备写入0
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // 设备写入状态
  disk.desc[idx[2]].next = 0;

  // 为virtio_disk_intr()记录struct buf
  b->disk = 1;
  disk.info[idx[0]].b = b;

  // 告诉设备描述符链中的第一个索引。
  disk.avail->ring[disk.avail->idx % NUM] = idx[0];

  __sync_synchronize();

  // 告诉设备另一个可用环条目可用。
  disk.avail->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // 值为队列号

  // 等待virtio_disk_intr()表示请求已完成。
  while(b->disk == 1) {
    sleep(b, &disk.vdisk_lock);
  }

  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);

  release(&disk.vdisk_lock);
}

void
virtio_disk_intr()
{
  acquire(&disk.vdisk_lock);

  // 设备不会发起另外个中断直到我们告诉它
  // 我们能看到这个中断是因为接下来的那一行做的
  // 这可能与设备将新条目写入“已用（used）”环的情况发生竞争，
  // 在这种情况下，我们可以在这个中断中处理新的完成条目，
  // 而在下一个中断中没有任何事情可做，这是无害的。
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  // 当设备加一个条目到已用环的时候，设备增加disk.used->idx
  while(disk.used_idx != disk.used->idx){
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if(disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = disk.info[id].b;
    b->disk = 0;   // 磁盘已完成buf
    wakeup(b);

    disk.used_idx += 1;
  }

  release(&disk.vdisk_lock);
}
