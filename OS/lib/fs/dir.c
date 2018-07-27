#include "dir.h"
#include "file.h"
#include "../device/ide.h"
#include "super_block.h"
#include "fs.h"
struct dir root_dir;
void open_root_dir(struct partition* part) {
	root_dir.inode = inode_open(part, part->sb->root_inode_no);
	root_dir.dir_pos = 0;
}

struct dir* dir_open(struct partition* part, uint32_t inode_no) {
	//printk("dir sys malloc start!!!\n");
	struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
	//printk("dir sys malloc end!!!\n");
	if (pdir == NULL) {
		printk("pdir alloc is failed!!!!\n");
		return NULL;
	}
	//printk("inode_open start!!!!\n");
	pdir->inode = inode_open(part, inode_no);
	//printk("inode_open end!!!!\n");
	pdir->dir_pos = 0;
	//printk("return !! 0x%x\n", (uint32_t)pdir);
	return pdir;
}

//在part分区内查找名为name的文件或目录，找到后返回true 并将目录项存入dir_e，否则返回false
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e) {
	uint32_t block_cnt = 140;
	uint32_t* all_block = (uint32_t*)sys_malloc(512 + 48);
	if (all_block == NULL) {
		printk("dir.h search_dir_entry alloc is error\n");
		return false;
	}
	uint32_t block_idx = 0;
	while (block_idx < 12) {
		all_block[block_idx] = pdir->inode->i_sectors[block_idx];
		block_idx++;
	}
	block_idx = 0;
	if (pdir->inode->i_sectors[12] != 0) {
		ide_read(part->my_disk, pdir->inode->i_sectors[12], all_block + 12, 1);
	}
	uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
	struct dir_entry* p_de = (struct dir_entry*)buf;
	uint32_t dir_entry_size = part->sb->dir_entry_size;
	uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

	block_idx = 0;
	while (block_idx < block_cnt) {
		if (all_block[block_idx] == 0) {
			block_idx++;
			continue;
		}
		ide_read(part->my_disk, all_block[block_idx], buf, 1);
		uint32_t dir_entry_idx = 0;
		while (dir_entry_idx < dir_entry_cnt) {
			if (!strcmp(p_de->filename, name)) {
				memcpy(dir_e, p_de, dir_entry_size);
				sys_free(buf);
				sys_free(all_block);
				return true;
			}
			p_de++;
			dir_entry_idx++;
		}
		block_idx++;
		p_de = (struct dir_entry*)buf;
		memset(buf, 0, SECTOR_SIZE);
	}
	sys_free(buf);
	sys_free(all_block);
	return false;
}

void dir_close(struct dir* dir) {
	if (dir == &root_dir) return;
	inode_close(dir->inode);
	sys_free(dir);
}

void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de) {
	ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
	memcpy(p_de->filename, filename, strlen(filename));
	p_de->i_no = inode_no;
	p_de->f_type = file_type;
}

//void aprintEsp(const char*);
//将p_de写入到父目录parent_dir 中，io_buf由主调函数提供
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf) {
	struct task_struct* cur_thread = running_thread();
ASSERT(cur_thread->stack_magic == 0x19870916);
	struct inode* dir_inode = parent_dir->inode;
ASSERT(cur_thread->stack_magic == 0x19870916);
	uint32_t dir_size = dir_inode->i_size;
ASSERT(cur_thread->stack_magic == 0x19870916);
	//printk("cur_part addr: 0x%x\n", (uint32_t)cur_part);
ASSERT(cur_thread->stack_magic == 0x19870916);
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

ASSERT(cur_thread->stack_magic == 0x19870916);
	ASSERT(dir_size % dir_entry_size == 0);
	uint32_t dir_entrys_per_sec = (512 / dir_entry_size);
	uint32_t block_lba = -1;

ASSERT(cur_thread->stack_magic == 0x19870916);
	struct dir_entry* dir_e = (struct dir_entry*)io_buf;
	uint8_t block_idx = 0;
	uint32_t all_block[140] = {0};
	while (block_idx < 12) {
		all_block[block_idx] = dir_inode->i_sectors[block_idx];
		block_idx++;
	}
	uint32_t block_bitmap_idx = -1;
ASSERT(cur_thread->stack_magic == 0x19870916);
	block_idx = 0;
	while (block_idx < 140) {
		block_bitmap_idx = -1;
		if (all_block[block_idx] == 0) {
			block_lba = block_bitmap_alloc(cur_part);
			if (block_lba == -1) {
				printk("dir.c alloc bitmap is error\n");
				return false;
			}
			block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;

			//清空申请的扇区
		//	memset(io_buf, 0, SECTOR_SIZE);
		//	ide_write(cur_part->my_disk, block_lba, io_buf, 1);

ASSERT(cur_thread->stack_magic == 0x19870916);
			ASSERT(block_bitmap_idx != -1);
			bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
			block_bitmap_idx = -1;
			if (block_idx < 12) {
				dir_inode->i_sectors[block_idx] = all_block[block_idx] = block_lba;
			} else if (block_idx == 12) {
				dir_inode->i_sectors[12] = block_lba;
				block_lba = -1;
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1) {
					block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
					bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
					dir_inode->i_sectors[12] = 0;
					printk("dir.c alloc block bitmap for sync is failed\n");
					return false;
				}
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				ASSERT(block_bitmap_idx != -1);
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
				all_block[12] = block_lba;
				ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_block + 12, 1);
			} else {
				all_block[block_idx] = block_lba;
				ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_block + 12, 1);
			}
			memset(io_buf, 0, 512);
			memcpy(io_buf, p_de, dir_entry_size);
			ide_write(cur_part->my_disk, all_block[block_idx], io_buf, 1);
			dir_inode->i_size += dir_entry_size;
			return true;
		}
		ide_read(cur_part->my_disk, all_block[block_idx], io_buf, 1);
		uint8_t dir_entry_idx = 0;
		while (dir_entry_idx < dir_entrys_per_sec) {
			//FT_UNKNOWN 为0 无论是初始化或者删除文件后 f_type 都会置为0
			if ((dir_e + dir_entry_idx)->f_type = FT_UNKNOWN) {
				memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
				ide_write(cur_part->my_disk, all_block[block_idx], io_buf, 1);
				dir_inode->i_size += dir_entry_size;
				return true;
			}
			dir_entry_idx++;
		}
		block_idx++;
	}
	printk("directory is full!!!\n");
	return false;
}

char* path_parse(char* pathname, char* name_store) {
	if (pathname[0] == '/') {
		while (*(++pathname) == '/');
	}
	while (*pathname != '/' && *pathname != 0) *name_store++ = *pathname++;
	if (pathname[0] == 0) return NULL;
	return pathname;
}

int32_t path_depth_cnt(char* pathname) {
	ASSERT(pathname != NULL);
	char* p = pathname;
	char name[MAX_FILE_NAME_LEN];
	p = path_parse(p, name);
	uint32_t depth = 0;
	while (name[0]) {
		depth++;
		memset(name, 0, MAX_FILE_NAME_LEN);
		if (p) p = path_parse(p, name);
	}
	return depth;
}


bool delete_dir_entry(struct partition* part, struct dir* dir, uint32_t inode_no, void* io_buf) {
	struct inode* dir_inode = dir->inode;
	uint32_t block_idx = 0;
	uint32_t all_blocks[140];
	uint32_t block_cnt = 12;
	while (block_idx < 12) {
		all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
		block_idx++;
	}
	if (dir_inode->i_sectors[12] != 0) {
		ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
		block_cnt = 140;
	}
	uint32_t dir_entry_size = part->sb->dir_entry_size;
	uint32_t dir_entry_sec = BLOCK_SIZE / dir_entry_size;

	struct dir_entry* dir_e = (struct dir_entry*)io_buf;
	struct dir_entry* dir_entry_found = NULL;
	bool is_dir_first_block = false;
	uint8_t dir_entry_idx, dir_entry_cnt;
	block_idx = 0;
	while (block_idx < block_cnt) {
		is_dir_first_block = false;
		if (all_blocks[block_idx] == 0) {
			block_idx++;
			continue;
		}
		dir_entry_idx = dir_entry_cnt = 0;
		memset(io_buf, 0, sizeof(io_buf));
		ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);
		while (dir_entry_idx < dir_entry_sec) {
			if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN) {
				if (!strcmp((dir_e + dir_entry_idx)->filename, ".")) {
					is_dir_first_block = true;
				} else if (strcmp((dir_e + dir_entry_idx)->filename, ".") && strcmp((dir_e + dir_entry_idx)->filename, "..")) {
					dir_entry_cnt++;
					if ((dir_e + dir_entry_idx)->i_no == inode_no) {
					    ASSERT(dir_entry_found == NULL);
					    dir_entry_found = dir_e + dir_entry_idx;
					}
				}
			}
			dir_entry_idx++;
		}
		if (dir_entry_found == NULL) {
			block_idx++;
			continue;
		}
		ASSERT(dir_entry_cnt >= 1);
		if (dir_entry_cnt == 1 && !is_dir_first_block) {
			uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
			bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
			bitmap_sync(part, block_bitmap_idx, BLOCK_BITMAP);

			if (block_idx < 12) {
				dir_inode->i_sectors[block_idx] = 0;
			} else {
				uint32_t indirect_blocks = 0;
				uint32_t indirect_block_idx = 12;
				while (indirect_block_idx < 140) {
					if (all_blocks[indirect_block_idx] != 0) indirect_blocks++;
					indirect_block_idx++;
				}
				ASSERT(indirect_blocks > 0);
				if (indirect_blocks > 1) {
					all_blocks[block_idx] = 0;
					ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
				} else {
					block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
					bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
					bitmap_sync(part, block_bitmap_idx, BLOCK_BITMAP);
					dir_inode->i_sectors[12] = 0;
				}
			}
		} else {
			memset(dir_entry_found, 0, dir_entry_size);
			ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
		}
		ASSERT(dir_inode->i_size >= dir_entry_size);
		dir_inode->i_size -= dir_entry_size;
		memset(io_buf, 0, SECTOR_SIZE * 2);
		inode_sync(part, dir_inode, io_buf);
		return true;
	}
	return false;
}

struct dir_entry* dir_read(struct dir* dir) {
	struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;
	struct inode* dir_inode = dir->inode;
	uint32_t all_blocks[140] = {0};
	uint32_t block_cnt = 12;
	uint32_t block_idx = 0, dir_entry_idx = 0;
	while (block_idx < 12) {
		all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
		block_idx++;
	}
	if (dir_inode->i_sectors[12] != 0) {
		ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
		block_cnt = 140;
	}
	block_idx = 0;
	uint32_t cur_dir_entry_pos = 0;
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
	uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
	while (block_idx < block_cnt && dir->dir_pos < dir->inode->i_size) {
		if (all_blocks[block_idx] == 0) {
			block_idx++;
			continue;
		}
		memset(dir_e, 0, SECTOR_SIZE);
		ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);
		dir_entry_idx = 0;
		while (dir_entry_idx < dir_entrys_per_sec) {
			if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN) {
				dir_entry_idx++;
				continue;
			}
			if (cur_dir_entry_pos < dir->dir_pos) {
				cur_dir_entry_pos += dir_entry_size;
				dir_entry_idx++;
				continue;
			}
			ASSERT(cur_dir_entry_pos == dir->dir_pos);
			cur_dir_entry_pos += dir_entry_size;
			dir->dir_pos += dir_entry_size;
			return dir_e + dir_entry_idx;
		}
		block_idx++;
	}
	return NULL;
}

bool dir_is_empty(struct dir* dir) {
	struct inode* dir_inode = dir->inode;
	return (dir_inode->i_size == cur_part->sb->dir_entry_size * 2);
}

//从父目录中parent_dir 删除子目录 child_dir;
int32_t dir_remove(struct dir* parent_dir, struct dir* child_dir) {
	struct inode* child_dir_inode = child_dir->inode;
	int32_t block_idx = 1;
	while (block_idx < 13) {
		ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
		block_idx++;
	}
	void* io_buf = sys_malloc(SECTOR_SIZE * 2);
	if (io_buf == NULL) {
		printk("dir_remove: malloc for io_buf failed!!!!\n");
		return -1;
	}
	if (delete_dir_entry(cur_part, parent_dir, child_dir->inode->i_no, io_buf)) {
		inode_release(cur_part, child_dir_inode->i_no);
	}
	sys_free(io_buf);
	return 0;
}

int32_t sys_rmdir(const char* pathname) {
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(pathname, &searched_record);
	printk("sys_rmdir searched_record patth: %s inode_no %d\n", searched_record.searched_path, inode_no);
	int retval = -1;
	if (inode_no == -1) {
		printk("In %s, sub path %s not exist \n", pathname, searched_record.searched_path);
	} else {
		if (searched_record.file_type == FT_REGULAR) {
			printk("%s is regular file!!!\n", pathname);
		} else if (searched_record.file_type == FT_DIRECTORY) {
//			printk("open dir start!!!---------------------------\n");
//			printk("open dir start!!!---------------------------\n");
//			printk("open dir start!!!---------------------------\n");
			//while (1);
			struct dir* dir = dir_open(cur_part, inode_no);
//			printk("open dir done!!!\n");
			if (!dir_is_empty(dir)) {
				printk("dir %s is not empty , it is not allowed to delete a nonempty directory!!\n", pathname);
			} else {
				if (!dir_remove(searched_record.parent_dir, dir)) retval = 0;
			}
			dir_close(dir);
		} else {
			printk("cao ji ba wan yi si bu xiang\n");
		}
	}
	dir_close(searched_record.parent_dir);
	return retval;
}
