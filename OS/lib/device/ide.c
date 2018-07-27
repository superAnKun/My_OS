#include "ide.h"

#define reg_data(channel) (channel->base_port + 0)
#define reg_error(channel) (channel->base_port + 1)
#define reg_sect_cnt(channel) (channel->base_port + 2)
#define reg_lba_l(channel) (channel->base_port + 3)
#define reg_lba_m(channel) (channel->base_port + 4)
#define reg_lba_h(channel) (channel->base_port + 5)
#define reg_dev(channel) (channel->base_port + 6)
#define reg_status(channel) (channel->base_port + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_alt_status(channel) (channel->port->base + 0x206)
#define reg_ctl(channel) reg_alt_status(channel)

#define BIT_ALT_STAT_BSY 0x80
#define BIT_ALT_STAT_DRDY 0x40
#define BIT_ALT_STAT_DRQ 0x8
#define BIT_DEV_MBS 0xa0
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

#define CMD_IDENTIFY 0xec        //硬盘识别
#define CMD_READ_SECTOR 0x20      //读扇区
#define CMD_WRITE_SECTOR 0x30      //写扇区

#define max_lba ((80 * 1024 * 1024 / 512) - 1)  //可读写最大扇区数

uint8_t channel_cnt;
struct ide_channel channels[2];    //ide通道
struct list partition_list;

void intr_hd_handler(uint8_t irq_no);
static void identify_disk(struct disk* hd);
static void partition_scan(struct disk* hd, uint32_t ext_lba);
/*
void ide_init() {
	console_put_str("ide_init start\n");
	uint8_t hd_cnt = *((uint8_t*)(0x475));  //获取硬盘数量
	ASSERT(hd_cnt > 0);
	channel_cnt = DIV_ROUND_UP(hd_cnt, 2);

	struct ide_channel* channel;
	uint8_t channel_no = 0;
	while (channel_no < channel_cnt) {
		channel = &channel[channel_no];
		sprintf(channel->name, "ide%d", channel_no);
		switch (channel_no) {
			case 0:
				channel->base_port = 0x1f0;
				channel->irq_no = 0x12 + 14;
				break;
			case 1:
				channel->base_port = 0x170;
				channel->irq_no = 0x20 + 15;
				break;
		}
		channel->expecting_intr = false;
		lock_init(&channel->lock, 1);
		sema_init(&channel->disk_done, 0);
	    uint32_t dev_no = 0;
	    while (dev_no < 2) {
		    struct disk* hd = &channel->devices[dev_no];
		    hd->my_channel = channel;
		    hd->dev_no = dev_no;
			sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
			identify_disk(hd);
			if (dev_no != 0) partition_scan(hd, 0); //内核本身的裸硬盘不处理
			dev_no++;
	    }
		dev_no = 0;
		register_handler(channel->irq_no, intr_hd_handler);
		channel_no++;
	}
	printf("\n    all partition info\n");
	list_traversal(&partition_list, partition_info, (int)NULL);
	console_put_str("ide_init done\n");
}
*/
static select_disk(struct disk* hd) {
	uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
	if (hd->dev_no == 1) {
		reg_device |= BIT_DEV_DEV;
	}
	outb(reg_dev(hd->my_channel), reg_device);
}

//向硬盘控制器写入起始扇区地址及要读写的扇区数
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
	ASSERT(lba <= max_lba);
	struct ide_channel* channel = hd->my_channel;
	outb(reg_sect_cnt(channel), sec_cnt);
	outb(reg_lba_l(channel), lba);
	outb(reg_lba_m(channel), lba >> 8);
	outb(reg_lba_h(channel), lba >> 16);
	outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
	channel->expecting_intr = true;
	outb(reg_cmd(channel), cmd);
}

static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
	uint32_t size_in_byte;
	if (sec_cnt == 0) {
		size_in_byte = 256 * 512;
	} else {
		size_in_byte = sec_cnt * 512;
	}
	insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

static void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
	uint32_t size_in_byte;
	if (sec_cnt == 0) {
		size_in_byte = 256 * 512;
	} else {
		size_in_byte = sec_cnt * 512;
	}
	outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

static bool busy_wait(struct disk* hd) {
	struct ide_channel* channel = hd->my_channel;
	uint16_t time_limit = 30 * 1000;
	while (time_limit -= 10 >= 0) {
		if (!(inb(reg_status(channel)) & BIT_ALT_STAT_BSY)) {
			return (inb(reg_status(channel)) & BIT_ALT_STAT_DRQ);
		} else {
			mtime_sleep(10);
		}
	}
	return false;
}

void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
	//printk("lba: %d\n", lba);
	ASSERT(lba <= max_lba);
	ASSERT(sec_cnt > 0);
	//printk("error11111 channel_addr: 0x%x\n", (uint32_t)buf);
	//printk("zheng chang \n");
	lock_acquire(&hd->my_channel->lock);
	/*
	if ((uint32_t)buf == 0x804840c) {
	    lock_acquire_test(&hd->my_channel->lock);
	} else {
	    lock_acquire(&hd->my_channel->lock);
	}
	*/
	//printk("error2222\n");
	select_disk(hd);
	uint32_t sect_op;
	uint32_t sect_done = 0;
	while (sect_done < sec_cnt) {
		if (sect_done + 256 <= sec_cnt) {
			sect_op = 256;
		} else {
			sect_op = sec_cnt - sect_done;
		}
		select_sector(hd, lba + sect_done, sect_op);
		cmd_out(hd->my_channel, CMD_READ_SECTOR);
		sema_down(&hd->my_channel->disk_done);
		if (!busy_wait(hd)) {
			char error[64];
			sprintf(error, "%s read sector %d failed!!!!\n", hd->name, lba);
			PANIC(error);
		}
		read_from_sector(hd, (void*)((uint32_t)buf + sect_done * 512), sect_op);
		//sema_down(&hd->my_channel->disk_done);   //读扇区的时候阻塞自己
		sect_done += sect_op;
	}
	lock_release(&hd->my_channel->lock);
}

void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
	ASSERT(lba <= max_lba);
	ASSERT(sec_cnt > 0);
	lock_acquire(&hd->my_channel->lock);
	select_disk(hd);
	uint32_t sect_op;
	uint32_t sect_done = 0;
	while (sect_done < sec_cnt) {
		if (sect_done + 256 <= sec_cnt) {
			sect_op = 256;
		} else {
			sect_op = sec_cnt - sect_done;
		}

		select_sector(hd, lba + sect_done, sect_op);
		cmd_out(hd->my_channel, CMD_WRITE_SECTOR);
		//sema_down(&hd->my_channel->disk_done);
		if (!busy_wait(hd)) {
			char error[64];
			sprintf(error, "%s read sector %d failed!!!!\n", hd->name, lba);
			PANIC(error);
		}
		write2sector(hd, (void*)((uint32_t)buf + sect_done * 512), sect_op);
		sema_down(&hd->my_channel->disk_done);   //读扇区的时候阻塞自己
		sect_done += sect_op;
	}
	lock_release(&hd->my_channel->lock);
}


//硬盘中断处理程序
void intr_hd_handler(uint8_t irq_no) {
	ASSERT(irq_no == 0x2e || irq_no == 0x2f);
	uint8_t ch_no = irq_no - 0x2e;
	struct ide_channel* channel = &channels[ch_no];
	ASSERT(channel->irq_no == irq_no);
	if (channel->expecting_intr) {
		channel->expecting_intr = false;
		sema_up(&channel->disk_done);
		inb(reg_status(channel));
	}
}




//16字节大小的结构题 用来存储分区表项
struct partition_table_entry {
	uint8_t bootable;
	uint8_t start_head;
	uint8_t start_sec;
	uint8_t start_chs;
	uint8_t fs_type;
	uint8_t end_head;
	uint8_t end_sec;
	uint8_t end_chs;
	uint32_t start_lba;     //本分区起始扇区的lba地址
	uint32_t sec_cnt;       //本分区的扇区数目 
} __attribute__ ((packed));


//MBR 或者 EBR
struct boot_sector {
	uint8_t other[446];
	struct partition_table_entry partition_table[4];
	uint16_t signatture;
} __attribute__ ((packed));

static void swap_pairs_bytes (const char* dst, char* buf, uint32_t len) {
	uint8_t idx;
	for (idx = 0; idx < len; idx += 2) {
		buf[idx + 1] = *dst++;
		buf[idx] = *dst++;
	}
	buf[idx] = '\0';
}

static void identify_disk(struct disk* hd) {
	char id_info[512];
	select_disk(hd);
	cmd_out(hd->my_channel, CMD_IDENTIFY);
	sema_down(&hd->my_channel->disk_done);
	if (!busy_wait(hd)) {
		char error[64];
		sprintf(error, "%s identify failed !!!!!\n", hd->name);
		PANIC(error);
	}
	read_from_sector(hd, id_info, 1);
	char buf[64];
	uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
	swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
	printk(" disk %s info: \n   SN: %s\n", hd->name, buf);
	memset(buf, 0, sizeof(buf));
	swap_pairs_bytes(&id_info[md_start], buf, md_len);
	printk("    MODULE: %s\n", buf);
	uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
	printk("    SECTORS: %d\n", sectors);
	printk("    CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}


static int32_t ext_lba_base = 0;
static uint8_t p_no = 0, l_no = 0;

static void partition_scan(struct disk* hd, uint32_t ext_lba) {
	//printk("ext_lba_base: %d p_no: %d, l_no: %d\n", ext_lba_base, p_no, l_no);
	struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
	ide_read(hd, ext_lba, bs, 1);
	uint8_t part_idx = 0;
	struct partition_table_entry* p = bs->partition_table;
	while (part_idx++ < 4) {
	   // printk("fs_type: %x\n", p->fs_type);
		if (p->fs_type == 0x5) {
			if (ext_lba_base != 0) {
				//printk("");
				partition_scan(hd, ext_lba_base + p->start_lba);
			} else {
				ext_lba_base = p->start_lba;
				partition_scan(hd, p->start_lba);
			}
		} else if (p->fs_type != 0) {
			if (ext_lba == 0) {
				hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
				hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
				hd->prim_parts[p_no].my_disk = hd;
				list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
				sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
				p_no++;
				//printk("p_no: %d cnt\n", p_no);
		//		if (p_no >= 4) printk("p_no: %d\n", p_no);
				ASSERT(p_no < 4);
			} else {
				hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
				hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
				hd->logic_parts[l_no].my_disk = hd;
				list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
				sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no + 5);
				l_no++;
				if (l_no >= 8) return;
			}
		}
		p++;
	}
	sys_free(bs);
}

static bool partition_info(struct list_elem* pelem, int arg) {
	struct partition* part = elem2entry(struct partition, part_tag, pelem);
	printk("   %s start_lba:0x%x, sec_cnt:0x%x\n", part->name, part->start_lba, part->sec_cnt);
	return false;
}



void ide_init() {
	console_put_str("ide_init start\n");
	ext_lba_base = 0;
	p_no = 0;
	l_no = 0;
	memset(channels, 0, sizeof(struct ide_channel) * 2);
	uint8_t hd_cnt = *((uint8_t*)(0x475));  //获取硬盘数量
	ASSERT(hd_cnt > 0);
	channel_cnt = DIV_ROUND_UP(hd_cnt, 2);
	struct ide_channel* channel_p;
	uint8_t channel_no = 0;
	list_init(&partition_list);
	while (channel_no < channel_cnt) {
		channel_p = &channels[channel_no];
		sprintf(channel_p->name, "ide%d", channel_no);
		switch (channel_no) {
			case 0:
				channel_p->base_port = 0x1f0;
				channel_p->irq_no = 0x20 + 14;
				break;
			case 1:
				channel_p->base_port = 0x170;
				channel_p->irq_no = 0x20 + 15;
				break;
		}
		channel_p->expecting_intr = false;
		lock_init(&channel_p->lock, 1);
		sema_init(&channel_p->disk_done, 0);
		register_handler(channel_p->irq_no, intr_hd_handler);
	    uint32_t dev_no = 0;
	    while (dev_no < 2) {
		    struct disk* hd = &channel_p->devices[dev_no];
		    hd->my_channel = channel_p;
		    hd->dev_no = dev_no;
			sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
			identify_disk(hd);
			if (dev_no != 0) partition_scan(hd, 0); //内核本身的裸硬盘不处理
			dev_no++;
	    }
		dev_no = 0;
		//register_handler(channel_p->irq_no, intr_hd_handler);
		channel_no++;
		//printf("channel_no: %d\n", channel_no);
	}
	printf("\n    all partition info\n");
	list_traversal(&partition_list, partition_info, (int)NULL);
	console_put_str("ide_init done\n");
}
