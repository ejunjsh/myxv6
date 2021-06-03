struct buf {
  int valid;   // 是否已从磁盘读取数据?
  int disk;    // 磁盘“拥有”buf吗?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU 缓存列表
  struct buf *next;
  uchar data[BSIZE];
};

