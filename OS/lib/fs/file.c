#include "file.h"
#include "../thread/thread.h"
#include "../device/ide.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "../kernel/list.h"
//#include "fs.h"

struct file file_table[MAX_FILE_OPEN];
/*
static void printThreadInfo(char* str) {
	printk("%s\n", str);
    struct task_struct* cur = running_thread();
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g" (esp));
    printk("esp: 0x%x cur: 0x%x __fd: %d magic: 0x%x\n", esp, (uint32_t)cur, cur->fd_table[3], cur->stack_magic);
}
*/

int32_t get_free_slot_in_global() {
	uint32_t idx = 3;
	while (idx < MAX_FILE_OPEN) {
		if (file_table[idx].fd_inode == NULL) break;
		idx++;
	}
	if (idx == MAX_FILE_OPEN) {
		printk("exceed max open files\n");
		return -1;
	}
	return idx;
}

int32_t pcb_fd_install(uint32_t global_fd_idx) {
	struct task_struct* cur = running_thread();
	uint8_t local_fd_idx = 3;
	while (local_fd_idx < MAX_FILES_OPEN_PER_PROC) {
		if (cur->fd_table[local_fd_idx] == -1) {
			cur->fd_table[local_fd_idx] = global_fd_idx;
			break;
		}
		local_fd_idx++;
	}
	if (local_fd_idx == MAX_FILE_OPEN) {
		printk("file.c 34 exceed max open files_per_proc\n");
		return -1;
	}
	return local_fd_idx;
}

//inode_bitmap 的一个位置为1  返回inode_table的下标
int32_t inode_bitmap_alloc(struct partition* part) {
	int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
	if (bit_idx == -1) return -1;
	bitmap_set(&part->inode_bitmap, bit_idx, 1);
	return bit_idx;
}


//block_bitmap 的一个位置为1 返回分配的扇区起始地址
int32_t block_bitmap_alloc(struct partition* part) {
	int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
	if (bit_idx == -1) return -1;
	bitmap_set(&part->block_bitmap, bit_idx, 1);
	return (part->sb->data_start_lba + bit_idx);
}

//将内存中bitmap 第bit_idx位所在的512字节同步到硬盘
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp) {
	uint32_t off_sec = bit_idx / 4096;          //扇区偏移
	uint32_t off_size = off_sec * BLOCK_SIZE;   //字节偏移
	uint32_t sec_lba;
	uint8_t* bitmap_off;
	switch (btmp) {
		case INODE_BITMAP:
			sec_lba = part->sb->inode_bitmap_lba + off_sec;
			bitmap_off = part->inode_bitmap.bits + off_size;
			break;
		case BLOCK_BITMAP:
			sec_lba = part->sb->block_bitmap_lba + off_sec;
			bitmap_off = part->block_bitmap.bits + off_size;
			break;
	}
	ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

void printEsp(const char*);
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {
	void* io_buf = sys_malloc(1024);
	if (io_buf == NULL) {
		printk("in file.c file_create: sys_malloc for io_buf failed!!!\n");
		return -1;
	}
	uint8_t rollback_step = 0;
	int32_t inode_no = inode_bitmap_alloc(cur_part);
	if (inode_no == -1) {
		printk("in file.c file_create: allocate inode failed!!!!\n");
		sys_free(io_buf);
		return -1;
	}
	struct inode* new_inode = NULL;
	struct task_struct* cur = running_thread();
	uint32_t* cur_pagedir = cur->pgdir;
	cur->pgdir = NULL;
	new_inode = (struct inode*)sys_malloc(sizeof(struct inode));
	cur->pgdir = cur_pagedir;
	if (new_inode == NULL) {
		printk("in file.c file_create alloc inode failed!!!\n");
		rollback_step = 1;
		goto rollback;
	}
	inode_init(inode_no, new_inode);
	uint32_t fd_idx = get_free_slot_in_global();
	if (fd_idx == -1) {
		printk("exceed max open");
		rollback_step = 2;
		goto rollback;
	}
	file_table[fd_idx].fd_inode = new_inode;
	file_table[fd_idx].pos = 0;
	file_table[fd_idx].fd_flag = flag;
	file_table[fd_idx].fd_inode->write_deny = false;

	struct dir_entry new_dir_entry;
	memset(&new_dir_entry, 0, sizeof(struct dir_entry));
	printk("filename: %s, inode_no:%d", filename, inode_no);
	create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);
	if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
		printk("sync dir_entry to disk is failed!!!\n");
		rollback_step = 3;
		goto rollback;
	}
	memset(io_buf, 0, 1024);
	inode_sync(cur_part, parent_dir->inode, io_buf);
	memset(io_buf, 0, 1024);
	inode_sync(cur_part, new_inode, io_buf);
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);
	list_push(&cur_part->open_inodes, &new_inode->inode_tag);
	new_inode->i_open_cnts = 1;
	sys_free(io_buf);
	return pcb_fd_install(fd_idx);
rollback:
	switch (rollback_step) {
		case 3:
			memset(&file_table[fd_idx], 0, sizeof(struct file));
		case 2:		
	        cur = running_thread();
	        uint32_t* cur_pagedir = cur->pgdir;
	        cur->pgdir = NULL;
			sys_free(new_inode);
	        cur->pgdir = cur_pagedir;
		case 1:
			bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
			break;
	}
	sys_free(io_buf);
	return -1;
}

uint32_t file_open(uint32_t inode_no, uint8_t flag) {
	ASSERT(inode_no > 2 && inode_no < 4096);
	int fd_idx = get_free_slot_in_global();
	if (fd_idx == -1) {
		printk("exceed max open files\n");
		return -1;
	}
	file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
	file_table[fd_idx].pos = 0;
	file_table[fd_idx].fd_flag = flag;
	bool write_deny = file_table[fd_idx].fd_inode->write_deny;
	if ((flag & O_WRONLY) || (flag & O_RDWR)) {
		enum intr_status old_status = intr_disable();
		if (!write_deny) {  //若当前没有其他进程写文件
			file_table[fd_idx].fd_inode->write_deny = true;
			intr_set_status(old_status);
		} else {
			intr_set_status(old_status);
			printk("file can't be write now, try again later\n");
			return -1;
		}
	}
	return pcb_fd_install(fd_idx);  //返回文件描述符
}


int32_t file_close(struct file* f) {
	if (f == NULL) {
		printk("file.c file_close is bad file is NULL\n");
		return -1;
	} else if (f->fd_inode == NULL) {
		printk("file.c file_close is bad file is NULL\n");
		return -1;
	}
	f->fd_inode->write_deny = false;
	inode_close(f->fd_inode);
	f->fd_inode = NULL;
	return 0;
}

uint32_t fd_locall2global(uint32_t local_fd) {
	struct task_struct* cur = running_thread();
	uint32_t global_fd = cur->fd_table[local_fd];
	ASSERT(global_fd > 2 && global_fd < MAX_FILE_OPEN);
	return (uint32_t)global_fd;
}

int32_t sys_close(int32_t fd) {
	int32_t ret = -1;
	if (fd >2) {
		uint32_t _fd = fd_locall2global(fd);
		if (is_pipe(fd)) {
			if (--file_table[_fd].pos == 0) {
				mfree_page(PF_KERNEL, file_table[_fd].fd_inode, 1);
				file_table[_fd].fd_inode = NULL;
			}
			ret = 0;
		} else {
		    ret = file_close(&file_table[_fd]);
		}
		running_thread()->fd_table[fd] = -1;
	}
	return ret;
}

//把buf中的count个字节写入file 成功返回字节数 失败返回 -1
int32_t file_write(struct file* file, const void* buf, uint32_t count) {
	if (file->fd_inode->i_size + count > (BLOCK_SIZE * 140)) {
		printk("exceed max file_size 7168 bytes, write file failed!!!!!\n");
		return -1;
	}
	uint8_t* io_buf = sys_malloc(512);
	if (io_buf == NULL) {
		printk("file.c 211 file_write sys_malloc is failed!!!!\n");
		return -1;
	}
	uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
	if (all_blocks == NULL) {
		printk("file.c file_write all_blocks sys_malloc is failed!!!1\n");
		sys_free(all_blocks);
		sys_free(io_buf);
		return -1;
	}
	const uint8_t* src = buf;
	uint32_t bytes_written = 0;  //用来记录写入数据大小
	uint32_t size_left = count;  //用来记录未写入数据大小
	int32_t block_lba = -1;
	uint32_t block_bitmap_idx = 0; //用来记录block在block_bitmap中的索引

	uint32_t sec_idx;
	uint32_t sec_lba;
	uint32_t sec_off_bytes;
	uint32_t sec_left_bytes;
	uint32_t chunk_size;   //每次写入磁盘的数据块大小
	int32_t indirect_block_table; //用来获取一级间接表地址
	uint32_t block_idx; //块索引


 //fd_locall2global(3);


	//如果文件还没有分配扇区 则先分配一个扇区
	if (file->fd_inode->i_sectors[0] == 0) {
		block_lba = block_bitmap_alloc(cur_part);
		if (block_lba == -1) {
			printk("file_write in file.c block_bitmap_alloc failed!!!\n");
			sys_free(all_blocks);
			sys_free(io_buf);
			return -1;
		}
		file->fd_inode->i_sectors[0] = block_lba;
		block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
		ASSERT(block_bitmap_idx != 0);
		bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	}

	//写入count 个字节数， 该文件已经占用的块数
	uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
	//文件将要占用的块数
	uint32_t file_will_used_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
	ASSERT(file_will_used_blocks <= 140);
	uint32_t add_blocks = file_will_used_blocks - file_has_used_blocks;


//    fd_locall2global(3);


	if (add_blocks == 0) {
		if (file_will_used_blocks <= 12) {
			block_idx = file_has_used_blocks - 1;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
		} else {
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		}
	} else {
		if (file_will_used_blocks <= 12) {
			block_idx = file_has_used_blocks - 1;
			ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

			block_idx = file_has_used_blocks;
			while (block_idx < file_will_used_blocks) {
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1) {
					printk("file_write: block_bitmap_aloc block_lba is failed!!!!\n");
					sys_free(io_buf);
					sys_free(all_blocks);
					return -1;
				}
				ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
				file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_SIZE);
				block_idx++;
			}
		} else if (file_has_used_blocks <= 12 && file_will_used_blocks > 12) {
			block_idx = file_has_used_blocks - 1;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
			block_lba = block_bitmap_alloc(cur_part);
			if (block_lba == -1) {
				printk("file.c file_write block_bitmap_alloc for situation 2 failed!!!\n");
				sys_free(all_blocks);
				sys_free(io_buf);
				return -1;
			}
			ASSERT(file->fd_inode->i_sectors[12] == 0);
			indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;
			block_idx = file_has_used_blocks;
			while (block_idx < file_will_used_blocks) {
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1) {
					printk("file.c file_write: block_bitmap_alloc is failed!!!!\n");
					sys_free(all_blocks);
					sys_free(io_buf);
					return -1;
				}
				if (block_idx < 12) {
					ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
					file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
				} else {
					all_blocks[block_idx] = block_lba;
				}
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
				block_idx++;
			}
			ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		} else if (file_has_used_blocks > 12) {
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
			block_idx = file_has_used_blocks;
			while (block_idx < file_will_used_blocks) {
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1) {
					printk("file.c file_write: block_bitmap_alloc situation 3 is failed!!!\n");
					sys_free(io_buf);
					sys_free(all_blocks);
					return -1;
				}
				all_blocks[block_idx++] = block_lba;
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
			}
			ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		}
	}

	bool first_write_block = true;
	file->pos = file->fd_inode->i_size - 1;
	while (bytes_written < count) {
		memset(io_buf, 0, BLOCK_SIZE);
		sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
		sec_lba = all_blocks[sec_idx];
		sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
		sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

		chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
		if (first_write_block) {
			ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
			first_write_block = false;
		}
		memcpy(io_buf + sec_off_bytes, src, chunk_size);
		ide_write(cur_part->my_disk, sec_lba, io_buf, 1);
		printk("file write at lba 0x%x\n", sec_lba);
		src += chunk_size;
		file->fd_inode->i_size += chunk_size;
		file->pos += chunk_size;
		bytes_written += chunk_size;
		size_left -= chunk_size;
	}
/*	
	// 这件事 还没完
struct task_struct* cur = running_thread();
uint32_t esp;
asm ("mov %%esp, %0" : "=g" (esp));
printk("esp: 0x%x cur: 0x%x __fd: %d magic: 0x%x\n", esp, (uint32_t)cur, cur->fd_table[3], cur->stack_magic);
	//inode_sync(cur_part, file->fd_inode, io_buf);
	//cur = running_thread();

asm ("mov %%esp, %0" : "=g" (esp));
printk("esp: 0x%x cur: 0x%x __fd: %d magic: 0x%x\n", esp, (uint32_t)cur, cur->fd_table[3], cur->stack_magic);
	printk(";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n");
//fd_locall2global(3);
*/
	//printThreadInfo("file.c error111");
	inode_sync(cur_part, file->fd_inode, io_buf);
	sys_free(all_blocks);
	sys_free(io_buf);
	printk("all size: %d\n", file->fd_inode->i_size);
	return bytes_written;
}

//从文件file中读取count 个字节写入buf返回读出的字节数，若到文件尾则返回-1
int32_t file_read(struct file* file, void* buf, uint32_t count) {
	uint8_t* buf_dst = (uint8_t*)buf;
	uint32_t size = count, size_left = size;
	if ((file->pos + count) > file->fd_inode->i_size) {
		size = file->fd_inode->i_size - file->pos;
		size_left = size;
		if (size == 0) return -1;
	}
	uint8_t* io_buf = (uint8_t*)sys_malloc(BLOCK_SIZE);
	if (io_buf == NULL) {
		printk("file_read: sys_malloc for io_buf failed!!\n");
		return -1;
	}
	uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
	if (all_blocks == NULL) {
		sys_free(io_buf);
		printk("file_read: sys_malloc for all_blocks failed!!!!\n");
		return -1;
	}

	uint32_t block_read_start_idx = file->pos / BLOCK_SIZE;
	uint32_t block_read_end_idx = (file->pos + size) / BLOCK_SIZE;
	uint32_t read_blocks = block_read_start_idx - block_read_end_idx;
	ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);
	int32_t indirect_block_table;
	uint32_t block_idx;
	if (read_blocks == 0) {
		ASSERT(block_read_start_idx == block_read_end_idx);
		if (block_read_end_idx < 12) {
			block_idx = block_read_end_idx;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
		} else {
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		}
	} else {
		if (block_read_end_idx < 12) {
			block_idx = block_read_start_idx;
			while (block_idx <= block_read_end_idx) {
				all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
				block_idx++;
			}
		} else if (block_read_start_idx < 12 && block_read_end_idx >= 12) {
			block_idx = block_read_start_idx;
			indirect_block_table = file->fd_inode->i_sectors[12];
			while (block_idx <= block_read_end_idx) {
				all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
				block_idx++;
			}
			ASSERT(indirect_block_table != 0);
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		} else {
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		}
	}
	uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
	uint32_t bytes_read = 0;
	while (bytes_read < size) {
		sec_idx = file->pos / BLOCK_SIZE;
		sec_lba = all_blocks[sec_idx];
		sec_off_bytes = file->pos % BLOCK_SIZE;
		sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
		chunk_size = size < sec_left_bytes ? size : sec_left_bytes;

		memset(io_buf, 0, BLOCK_SIZE);
		ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
		memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);

		buf_dst += chunk_size;
		file->pos += chunk_size;
		bytes_read += chunk_size;
		size -= chunk_size;
	}
	sys_free(io_buf);
	sys_free(all_blocks);
	return bytes_read;
}
