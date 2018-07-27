#include "inode.h"
#include "../device/ide.h"
#include "super_block.h"
#include "file.h"
struct inode_position {
	bool two_sec;
	uint32_t sec_lba;
	uint32_t off_size;
};

static void printThreadInfo(char* str) {
	printk("%s\n", str);
	struct task_struct* cur = running_thread();
	//cur->stack_magic = old_magic;
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g" (esp));
    printk("esp: 0x%x cur: 0x%x __fd: %d\n", esp, (uint32_t)cur, cur->fd_table[3]);
	printk("addr stack_magic: 0x%x  value: 0x%x\n", (uint32_t)&cur->stack_magic, cur->stack_magic);
}

//获取inode所在扇区 和 扇区内的偏移量
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
	ASSERT(inode_no < 4096);
	//printThreadInfo("error1111");
	uint32_t inode_table_lba = part->sb->inode_table_lba;

	uint32_t inode_size = sizeof(struct inode);
	uint32_t off_size = inode_no * inode_size;
	uint32_t off_sec = off_size / 512;
	uint32_t off_size_in_sec = off_size % 512;

	uint32_t left_in_sec = 512 - off_size_in_sec;
	if (left_in_sec < inode_size) {
		inode_pos->two_sec = true;
	} else {
		inode_pos->two_sec = false;
	}
	inode_pos->sec_lba = inode_table_lba + off_sec;
	inode_pos->off_size = off_size_in_sec;

	//printThreadInfo("error2222");
}

//将inode写入到分区part
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {
	//struct task_struct* cur = running_thread();
	//uint32_t old_magic = cur->stack_magic;
	//printThreadInfo("ddddddd11111");
	uint8_t inode_no = inode->i_no;
//	printk("ddd111: 0x%x\n", cur->stack_magic);
	//printThreadInfo("tttttttt2222");
	struct inode_position inode_pos;
//	printk("ddd2222: 0x%x\n", cur->stack_magic);
	//printThreadInfo("kkkkkkkkk333");
	inode_locate(part, inode_no, &inode_pos);
//	printk("ddd333: 0x%x\n", cur->stack_magic);
	//printThreadInfo("hhhhhhhh4444");
	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
//	printk("ddd4444: 0x%x\n", cur->stack_magic);
	struct inode pure_inode;
//	printk("ddd5555: 0x%x\n", cur->stack_magic);
	memcpy(&pure_inode, inode, sizeof(struct inode));

//	printk("ddd6666: 0x%x\n", cur->stack_magic);
	pure_inode.i_open_cnts = 0;
	pure_inode.write_deny = false;
	pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

//未知的原因 导致PCB结构数据发生变化----------------------------------------------------
	//struct task_struct* cur = running_thread();
	//cur->stack_magic = old_magic;
//uint32_t esp;
//asm ("mov %%esp, %0" : "=g" (esp));
//printk("esp: 0x%x cur: 0x%x __fd: %d\n", esp, (uint32_t)cur, cur->fd_table[3]);
//	printk("stack_magic: 0x%x\n", cur->stack_magic);
//---------------------------------------------------------------------------------------
	char* inode_buf = (char*)io_buf;
	if (inode_pos.two_sec) {
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
		memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
		//将待写入的inode拼入到2个扇区中
		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
	} else {
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
		memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
	}
}

struct inode* inode_open(struct partition* part, uint32_t inode_no) {
	//printk("inode_open 1111\n");
	struct list_elem* elem = part->open_inodes.head.next;
	//printk("inode_open 2222\n");
	//ASSERT(1 == 2);
	struct inode* inode_found;
	//printk("inode_open 3333\n");
	while (elem != &part->open_inodes.tail) {
		inode_found = elem2entry(struct inode, inode_tag, elem);
		if (inode_found->i_no == inode_no) {
			inode_found->i_open_cnts++;
			return inode_found;
		}
		elem = elem->next;
	}

	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	struct task_struct* cur = running_thread();
	uint32_t* cur_pagedir_bak = cur->pgdir;
	cur->pgdir = NULL;
	inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
	//printk("inode_open 11 11 11\n");
	cur->pgdir = cur_pagedir_bak;
	//printk("inode_open 12 12 12\n");
	char* inode_buf;
	//printk("inode_open 13 13 13\n");
	if (inode_pos.two_sec) {
		inode_buf = (char*)sys_malloc(1024);
	//	printk("bu\n");
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
	} else {
		//printk("ke sec_lba: 0x%x lba name: %s\n", inode_pos.sec_lba, part->name);
		inode_buf = (char*)sys_malloc(512);
	//	printk("inode_open: 0x%x\n", (uint32_t)inode_buf);
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
		//printk("neng\n");
	}
	//printk("inode_open 14 14 14\n");
	memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));
	//printk("inode_open 15 15 15\n");
	list_push(&part->open_inodes, &inode_found->inode_tag);
	//printk("inode_open 16 16 16\n");
	inode_found->i_open_cnts = 1;
	sys_free(inode_buf);
	return inode_found;
}

void inode_close(struct inode* inode) {
	enum intr_status old_status = intr_disable();
	if (--inode->i_open_cnts == 0) {
		list_remove(&inode->inode_tag);
		struct task_struct* cur = running_thread();
		uint32_t* cur_pagedir_bak = cur->pgdir;
		cur->pgdir = NULL;
		sys_free(inode);
		cur->pgdir = cur_pagedir_bak;
	}
	intr_set_status(old_status);
}

void inode_init(uint32_t inode_no, struct inode* new_inode) {
	new_inode->i_no = inode_no;
	new_inode->i_size = 0;
	new_inode->i_open_cnts = 0;
	new_inode->write_deny = false;

	uint8_t sec_idx = 0;
	while (sec_idx < 13) {
		new_inode->i_sectors[sec_idx] = 0;
		sec_idx++;
	}
}


//清空某个inode节点 测试用
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf) {
	ASSERT(inode_no < 4096);
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
	char* inode_buf = (char*)io_buf;
	if (inode_pos.two_sec) {
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
		memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
	} else {
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
		memset((inode_buf + inode_pos.sec_lba), 0, sizeof(struct inode));
		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
	}
}

void inode_release(struct partition* part, uint32_t inode_no) {
	struct inode* inode_to_del = inode_open(part, inode_no);
	uint32_t all_blocks[140] ={0};
	uint32_t block_idx = 0;
	uint32_t block_cnt = 12;
	uint32_t block_bitmap_idx;
	while (block_idx < 12) {
		all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
		block_idx++;
	}
	if (inode_to_del->i_sectors[12] != 0) {
		ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
		block_cnt = 140;

		block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
		ASSERT(block_bitmap_idx > 0);
		bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
		bitmap_sync(part, block_bitmap_idx, BLOCK_BITMAP);
	}
	block_idx = 0;
	while (block_idx < block_cnt) {
		if (all_blocks[block_idx] != 0) {
			block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
			ASSERT(block_bitmap_idx > 0);
			bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
			bitmap_sync(part, block_bitmap_idx, BLOCK_BITMAP);
		}
		block_idx++;
	}
	//回收当前节点
	bitmap_set(&part->inode_bitmap, inode_no, 0);
	bitmap_sync(part, inode_no, INODE_BITMAP);

	//清空当前节点
	void* io_buf = sys_malloc(1024);
	inode_delete(part, inode_no, io_buf);
	sys_free(io_buf);
	inode_close(inode_to_del);
}
