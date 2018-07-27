#ifndef __IDE_H___
#define __IDE_H___
#include "../stdint.h"
#include "../lib/stdio.h"
#include "../thread/thread.h"
#include "../kernel/io.h"
struct partition {
	uint32_t start_lba;   //分区 起始扇区
	uint32_t sec_cnt;      //扇区数
	struct disk* my_disk;     //所在硬盘
	struct list_elem part_tag;  //该分区在队列中的标记
	char name[8];               //该分区的名字
	struct super_block* sb;      //本分区的超级块 即操作系统一次性读取多个扇区的块
	struct bitmap block_bitmap;   //块位图
	struct bitmap inode_bitmap;   //i节点位图
	struct list open_inodes;       //本分区打开的i节点队列
};

struct disk {
	char name[8];       //硬盘名称
	struct ide_channel* my_channel;    //使用的通道
	uint8_t dev_no;                   //(0)主盘 or (1)从盘
	struct partition prim_parts[4];     //主分区 最多4个
	struct partition logic_parts[8];    //逻辑分区数量无上限 但最多8个
};

struct ide_channel {
	char name[8];     //通道名称
	uint16_t base_port;  //本通道的起始端口号
	uint8_t irq_no;  //本通道所使用的中断号
	struct lock lock;  //通道锁
	bool expecting_intr;   //表示等待硬盘的中断
	struct semaphore disk_done; //用于阻塞、唤醒驱动程序
	struct disk devices[2];  //一个通道的两个硬盘 主硬盘 从硬盘
};

extern uint8_t channel_cnt;
extern struct ide_channel channels[2];    //ide通道
extern struct list partition_list;

void ide_init();



#endif
