//
// virtio设备定义。对于mmio接口和virtio描述符。
// 仅用qemu测试。
// 这是“遗留”virtio接口。
//
// virtio规范请看：
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf

// Virtio mmio控制寄存器，映射起始于0x10001000。
// 来自于 qemu的 virtio_mmio.h
#define VIRTIO_MMIO_MAGIC_VALUE		0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION		0x004 // 版本; 1 is 遗留
#define VIRTIO_MMIO_DEVICE_ID		0x008 // 设备类型; 1 是网络, 2 是磁盘
#define VIRTIO_MMIO_VENDOR_ID		0x00c // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028 // PFN的页面大小，仅写
#define VIRTIO_MMIO_QUEUE_SEL		0x030 // 选择队列，仅写
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034 // 当前队列的最大大小，只读
#define VIRTIO_MMIO_QUEUE_NUM		0x038 // 当前队列的大小，仅写
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c // 使用环对齐，仅写
#define VIRTIO_MMIO_QUEUE_PFN		0x040 // 队列的物理页码，读/写
#define VIRTIO_MMIO_QUEUE_READY		0x044 // 就绪位
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050 // 仅写
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060 // 只读
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064 // 仅写
#define VIRTIO_MMIO_STATUS		0x070 // 读/写

// 状态寄存器位，来自于qemu的 virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8

// 设备功能位
#define VIRTIO_BLK_F_RO              5	/* 磁盘是只读 */
#define VIRTIO_BLK_F_SCSI            7	/* 支持scsi命令传递 */
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* 可在配置中使用写回模式 */
#define VIRTIO_BLK_F_MQ             12	/* 支持多个vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// virtio描述符数量
// 一定是二的幂。
#define NUM 8

// 一个描述符。来自于规范
struct virtq_desc {
  uint64 addr;
  uint32 len;
  uint16 flags;
  uint16 next;
};
#define VRING_DESC_F_NEXT  1 // 与另一个描述符链接
#define VRING_DESC_F_WRITE 2 // 设备写入（vs读取）

// （整个）可用环，来自于规范。
struct virtq_avail {
  uint16 flags; // 一直是0
  uint16 idx;   // 驱动程序接下来将写入ring[idx]
  uint16 ring[NUM]; // 链头描述符编号
  uint16 unused;
};

//“used”环中的一个条目，设备用它告诉驱动程序已完成的请求。
struct virtq_used_elem {
  uint32 id;   // 已完成描述符链的开始索引
  uint32 len;
};

struct virtq_used {
  uint16 flags; // 一直是0
  uint16 idx;   // 设备在添加ring[]项时递增
  struct virtq_used_elem ring[NUM];
};

// 这些特定于virtio块设备，例如磁盘，
// 如本规范第5.2节所述。

#define VIRTIO_BLK_T_IN  0 // 读磁盘
#define VIRTIO_BLK_T_OUT 1 // 写磁盘

// 磁盘请求中第一个描述符的格式。后跟两个包含块的描述符和一个单字节状态。
struct virtio_blk_req {
  uint32 type; // VIRTIO_BLK_T_IN or ..._OUT
  uint32 reserved;
  uint64 sector;
};
