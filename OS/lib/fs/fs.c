#include "fs.h"
#include "../device/ide.h"
#include "inode.h"
#include "dir.h"
#include "file.h"
#include "super_block.h"
#include "../device/ioqueue.h"
#include "../device/keyboard.h"
#include "../thread/thread.h"

struct partition* cur_part = NULL;
static void partition_format(struct partition* part) {
	uint32_t boot_sector_sects = 1;
	uint32_t super_block_sects = 1;
	uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);

	uint32_t inode_table_sects = DIV_ROUND_UP((sizeof(struct inode) * MAX_FILES_PER_PART), SECTOR_SIZE);

	uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;

	uint32_t free_sects = part->sec_cnt - used_sects;

	uint32_t block_bitmap_sects;
	block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR); //空闲块位图所占用的扇区数
	uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects; // 空闲位图的大小
	block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

	struct super_block sb;
	sb.magic = 0x19590318;
	sb.sec_cnt = part->sec_cnt;
	sb.inode_cnt = MAX_FILES_PER_PART;
	sb.part_lba_base = part->start_lba;

	sb.block_bitmap_lba = sb.part_lba_base + 2; //第0个是引导块 第1个是超级块
	sb.block_bitmap_sects = block_bitmap_sects;

	sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
	sb.inode_bitmap_sects = inode_bitmap_sects;

	sb.inode_table_lba = sb.inode_bitmap_lba + sb.block_bitmap_sects;
	sb.inode_table_sects = inode_table_sects;

	sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
	sb.root_inode_no = 0;
	sb.dir_entry_size = sizeof(struct dir_entry);

	printk("%s info:\n", part->name);
	printk("   magic:0x%x\n part_lba_base:0x%x\n all_sectors:0x:%x\n  inode_cnt:0x%x\n block_bitmap_lba:0x%x\n \
			block_bitmap_sectors:0x%x\n inode_bitmap_lba:0x%x\n inode_bitmap_sectors:0x%x\n inode_table_lba:0x%x\n \
			inode_table_sectors:0x%x\n data_start_lba:0x%x\n", sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, \
			sb.block_bitmap_lba, sb.block_bitmap_sects, sb.inode_bitmap_lba, sb.inode_bitmap_sects, sb.inode_table_lba, \
			sb.inode_table_sects, sb.data_start_lba);
	struct disk* hd = part->my_disk;
	ide_write(hd, part->start_lba + 1, &sb, 1); //将超级块写入磁盘

	uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);

	buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;
	uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

	buf[0] |= 0x01;  //
	uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
	uint32_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
	uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_bit % SECTOR_SIZE);
	memset(&buf[block_bitmap_last_byte], 0xff, last_size);
	uint8_t bit_idx = 0;
	while (bit_idx <= block_bitmap_last_bit) {
		buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
	}
	ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);
	memset(buf, 0, buf_size);
	buf[0] |= 0x1;
	ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

	memset(buf, 0, buf_size);
	struct inode* i = (struct inode*)buf;
	i->i_size = sb.dir_entry_size * 2;
	i->i_no = 0;
	i->i_sectors[0] = sb.data_start_lba;
	ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);  //将inode数组写入磁盘

	memset(buf, 0, buf_size);

	struct dir_entry* p_de = (struct dir_entry*)buf;

	memcpy(p_de->filename, ".", 1);
	p_de->i_no = 0;
	p_de->f_type = FT_DIRECTORY;
	p_de++;

	memcpy(p_de->filename, "..", 2);
	p_de->i_no = 0;
	p_de->f_type = FT_DIRECTORY;
	ide_write(hd, sb.data_start_lba, buf, 1);
	printk("   root_dir_lba:0x%x\n", sb.data_start_lba);
	printk("%s format done\n", part->name);
	sys_free(buf);
}

static bool mount_partition(struct list_elem* pelem, int arg) {
	char* part_name = (char*)arg;
	struct partition* part = elem2entry(struct partition, part_tag, pelem);
	printk("part->name: %s  part_name: %s \n", part->name, part_name);
	if (!strcmp(part->name, part_name)) {
		cur_part = part;
		struct disk* hd = cur_part->my_disk;
	//	struct super_block* sb_buf = (struct super_block*)sys_malloc(sizeof(struct super_block));
		cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
		if (cur_part->sb == NULL) PANIC("alloc memory failed!!!");
		memset(cur_part->sb, 0, sizeof(struct super_block));
		ide_read(hd, cur_part->start_lba + 1, cur_part->sb, 1);
		cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(cur_part->sb->block_bitmap_sects * SECTOR_SIZE);
		if (cur_part->block_bitmap.bits == NULL) PANIC("alloc memory failed!!!");
		cur_part->block_bitmap.btmp_bytes_len = cur_part->sb->block_bitmap_sects * SECTOR_SIZE;

		ide_read(hd, cur_part->sb->block_bitmap_lba, cur_part->block_bitmap.bits, cur_part->sb->block_bitmap_sects);

		cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sizeof(struct super_block) * SECTOR_SIZE);
		if (cur_part->inode_bitmap.bits == NULL) PANIC("alloc memory failed!!!!");
		cur_part->inode_bitmap.btmp_bytes_len = cur_part->sb->inode_bitmap_sects * SECTOR_SIZE;
		ide_read(hd, cur_part->sb->inode_bitmap_lba, cur_part->inode_bitmap.bits, cur_part->sb->inode_bitmap_sects);
		uint8_t *p = (uint8_t*)cur_part->inode_bitmap.bits;
		list_init(&cur_part->open_inodes);
		printk("mount %s done!\n", part->name);
		return true;
	}
	return false;
}

//搜索pathname 若找到返回inode号 否则返回-1
int search_file(const char* pathname, struct path_search_record* searched_record) {
	if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")) {
		searched_record->searched_path[0] = 0;
		searched_record->parent_dir = &root_dir;
		searched_record->file_type = FT_DIRECTORY;
		return 0;
	}
	uint32_t path_len = strlen(pathname);
	ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
	char* sub_path = (char*)pathname;
	struct dir* parent_dir = &root_dir;
	struct dir_entry dir_e;
	char name[MAX_FILE_NAME_LEN] = {0};
	searched_record->parent_dir = parent_dir;
	searched_record->file_type = FT_UNKNOWN;
	uint32_t parent_inode_no = 0;
	sub_path = path_parse(sub_path, name);
	while (name[0]) {
		ASSERT(strlen(searched_record->searched_path) < 512);
		strcat(searched_record->searched_path, "/");
		strcat(searched_record->searched_path, name);
		if (search_dir_entry(cur_part, parent_dir, name, &dir_e)) {
			memset(name, 0, MAX_FILE_NAME_LEN);
			if (sub_path) {
				sub_path = path_parse(sub_path, name);
			}
			if (dir_e.f_type == FT_DIRECTORY) {
				parent_inode_no = parent_dir->inode->i_no;
				dir_close(parent_dir);
				parent_dir = dir_open(cur_part, dir_e.i_no); //更新父目录
				searched_record->parent_dir = parent_dir;
				continue;
			} else if (dir_e.f_type == FT_REGULAR) {
				searched_record->file_type = FT_REGULAR;
				return dir_e.i_no;
			}
		} else {
			return -1;
		}
	}
	dir_close(searched_record->parent_dir);
	searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
	searched_record->file_type = FT_DIRECTORY;
	//若没有文件 返回目录
	return dir_e.i_no;
}

int32_t sys_open(const char* pathname, uint8_t flags) {
	if (pathname[strlen(pathname) - 1] == '/') {
		printk("can't open a directory %s\n", pathname);
		return -1;
	}
	ASSERT(flags <= 15);
	uint32_t fd = -1;
	struct path_search_record search_record;
	memset(&search_record, 0, sizeof(struct path_search_record));
	//printEsp("sys_open111 ");
	//printk("errror00000000\n");
	uint32_t pathname_depth = path_depth_cnt((char*)pathname);
	//printEsp("sys_open2222 ");
	//printk("errror111111\n");
	uint32_t inode_no = search_file(pathname, &search_record);
	//printEsp("sys_open3333 ");
	//printk("error2222222\n");
	bool found = inode_no != -1 ? true : false;
	if (search_record.file_type == FT_DIRECTORY) {
		printk("can't open a directory with open(), use opendir() to instead\n");
		dir_close(search_record.parent_dir);
		return -1;
	}
	//printEsp("sys_open44444 ");
	uint32_t path_searched_depth = path_depth_cnt((char*)search_record.searched_path);
	//printEsp("sys_open55555 ");
	if (pathname_depth != path_searched_depth) {
		printk("cantnot access %s: Not a directory, subpath %s is't exist\n", pathname, search_record.searched_path);
		dir_close(search_record.parent_dir);
		return -1;
	}
	//printEsp("sys_open6666 ");
	//若未找到文件 且 并不是创建文件 则 返回-1
	if (!found && !(flags & O_CREAT)) {
		printk("in path %s , file %s is't exist\n", search_record.searched_path, (strrchr(search_record.searched_path, '/') + 1));
		dir_close(search_record.parent_dir);
		return -1;
	} else if (found && (flags & O_CREAT) && (flags & O_RDWR)) {
		printk("%s has already exist!\n", pathname);
		dir_close(search_record.parent_dir);
		return -1;
	}
	switch (flags & O_CREAT) {
		case O_CREAT:
			printk("creating file\n");
	//		printEsp("sys_open7777 ");
			fd = file_create(search_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
	//		printEsp("sys_open8888 ");
			dir_close(search_record.parent_dir);
	//		printEsp("sys_open9999 ");
			//while (1);
			break;
		default:
			//默认情况下均为打开文件
			fd = file_open(inode_no, flags);
	}
	return fd;
}

void filesys_init() {
	uint8_t channel_no = 0, dev_no, part_idx = 0;
	struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
	if (sb_buf == NULL)  PANIC("alloc memory failed!!!!");
	printk("searching filesystem............\n");
	while (channel_no < channel_cnt) {
		dev_no = 0;
		while (dev_no < 2) {
			if (dev_no == 0) {
				dev_no++;
				continue;
			}
			struct disk* hd = &channels[channel_no].devices[dev_no];
			struct partition* part = hd->prim_parts;
			while (part_idx < 12) {
				if (part_idx == 4) part = hd->logic_parts;
				if (part->sec_cnt != 0) {   //分区存在
					memset(sb_buf, 0, sizeof(sb_buf));
					ide_read(hd, part->start_lba + 1, sb_buf, 1);
					if (sb_buf->magic == 0x19590318) {
						printk("%s has filesystem\n", part->name);
					} else {
						printk("formatting %s s partition %s .......\n", hd->name, part->name);
						partition_format(part);
					}
				}
				part_idx++;
				part++;
			}
			dev_no++;
		}
		channel_no++;
	}
	sys_free(sb_buf);
	char default_part[8] = "sdb1";
	list_traversal(&partition_list, mount_partition, (int)default_part);
	open_root_dir(cur_part);
	uint32_t fd_idx = 0;
	while (fd_idx < MAX_FILE_OPEN) {
		file_table[fd_idx++].fd_inode = NULL;
	}
}

int32_t sys_write(int32_t fd, const void* buf, uint32_t count) {
	if (fd < 0) {
		printk("sys_write: fd error~~~\n");
		return -1;
	}
	if (is_pipe(fd)) {
	//	printk("ddddddddddddddddddddddddddddddddddddddddd\n");
		return pipe_write(fd, buf, count);
	}
	if (fd == 1) {
		char temp_buf[1024];
		memcpy(temp_buf, buf, 1024);
		console_put_str(temp_buf);
		return count;
	}
	uint32_t _fd = fd_locall2global(fd);
	struct file* wr_file = &file_table[_fd];
	if ((wr_file->fd_flag & O_WRONLY) || (wr_file->fd_flag & O_RDWR)) {
		uint32_t bytes_written = file_write(wr_file, buf, count);
		return bytes_written;
	} else {
		console_put_str("sys_write: not allowed to write file without O_WRONLY or O_RDWR\n");
		return -1;
	}
}

int32_t sys_read(int32_t fd, void* buf, uint32_t count) {
	ASSERT(buf != NULL);
	int ret = -1;

	if (is_pipe(fd)) return pipe_read(fd, buf, count);
	if (fd < 0 || fd == stdout_no || fd == stderr_no) {
		printk("sys_read: fd error\n");
	} else if(fd == stdin_no) { 
		char* buffer = buf;
		uint32_t bytes_read = 0;
		while (bytes_read < count) {
			*buffer = ioq_getchar(&kbd_buf);
			bytes_read++;
			buffer++;
		}
		ret = (bytes_read == 0 ? -1 : (int32_t)bytes_read);
	} else {
	    uint32_t _fd = fd_locall2global(fd);
	 //   if (_fd == 3) printk("_fd: %d MAX_FILE_OPEN: %d\n", _fd, MAX_FILE_OPEN);
	    ASSERT(_fd < MAX_FILE_OPEN);
		ret = file_read(&file_table[_fd], buf, count);
	}
	return ret;
}

int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
	if (fd < 0) {
		printk("file.c sys_lseek fd is bad!!!\n");
		return -1;
	}
	ASSERT(whence > 0 && whence < 4);
	uint32_t _fd = fd_locall2global(fd);
	struct file* pf = &file_table[_fd];
	int32_t new_pos = 0;
	int32_t file_size = (int32_t)pf->fd_inode->i_size;
	switch (whence) {
		case SEEK_SET:
			new_pos = offset;
			break;
		case SEEK_CUR:
			new_pos = (int32_t)pf->pos + offset;
			break;
		case SEEK_END:
			new_pos = file_size + offset;
			break;
	}
	if (new_pos < 0 || new_pos > (file_size - 1)) return -1;
	pf->pos = new_pos;
	return new_pos;
}

void printEsp(const char* info) {
	/*
	uint32_t esp;
	struct task_struct* cur_thread = running_thread();
	asm ("mov %%esp, %0" : "=g" (esp));
	printk("%s: esp 0x%x stack_magic: 0x%x cur_threadaddr:0x%x\n", info, esp, (uint32_t)cur_thread->stack_magic, \
			(uint32_t)cur_thread);
			*/
}

int32_t sys_unlink(const char* pathname) {
	ASSERT(strlen(pathname) < MAX_PATH_LEN);

	//printEsp("sys_unlink333: ");
	//while (1);
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int32_t inode_no = search_file(pathname, &searched_record);
	ASSERT(inode_no != 0);
	if (inode_no == -1) {
		printk("file: %s is not found!\n", pathname);
		dir_close(searched_record.parent_dir);
		return -1;
	}
	printk("sys_unlink inode_no: %d \n", inode_no);
	if (searched_record.file_type == FT_DIRECTORY) {
		printk("can't delete a directory with unlink()");
		dir_close(searched_record.parent_dir);
		return -1;
	}


	//查看文件是否已经打开
	uint32_t file_idx = 0;
	while (file_idx < MAX_FILE_OPEN) {
		if (file_table[file_idx].fd_inode != NULL && file_table[file_idx].fd_inode->i_no == (uint32_t)inode_no) break;
		file_idx++;
	}

	if (file_idx < MAX_FILE_OPEN) {
		printk("file: %s is in use, not allow to delete\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}
	ASSERT(file_idx == MAX_FILE_OPEN);
	void* io_buf = sys_malloc(SECTOR_SIZE * 2);
	if (io_buf == NULL) {
		printk("sys_ulink: malloc for io_buf is failed!!!\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}
	//printEsp("sys_unlink111 ");
	struct dir* parent_dir = searched_record.parent_dir;
	//printEsp("sys_unlink222 ");
	delete_dir_entry(cur_part, parent_dir, inode_no, io_buf);
	//printEsp("sys_unlink333 ");
	inode_release(cur_part, inode_no);
	sys_free(io_buf);
	dir_close(parent_dir);
	return 0;
}

uint32_t sys_mkdir(const char* filename) {
	struct task_struct* cur_thread = running_thread();


	uint8_t rollback_step = 0;
	void* io_buf = sys_malloc(SECTOR_SIZE * 2);
	if (io_buf == NULL) {
		printk("sys_mkdir: io_buf alloc is failed!!!\n");
		return -1;
	}
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = -1;
	inode_no = search_file(filename, &searched_record);
	if (inode_no != -1) {
		printk("file or directory: %s exist\n", filename);
		rollback_step = 1;
		goto rollback;
	}
	uint32_t pathname_depth = path_depth_cnt((char*)filename);
	uint32_t searched_depth = path_depth_cnt((char*)searched_record.searched_path);
	if (pathname_depth != searched_depth) {
		printk("file for directory can't access %s: Not a directory , subpath is exit!!\n", searched_record.searched_path);
		rollback_step = 1;
		goto rollback;
	}
	inode_no = inode_bitmap_alloc(cur_part);
	if (inode_no == -1) {
		printk("fs.c inode_no get alloc is failed!!!!\n");
		rollback_step = 1;
		goto rollback;
	}
	struct inode new_dir_inode;
	inode_init(inode_no, &new_dir_inode);

	uint32_t block_bitmap_idx = 0;
	int32_t block_lba = -1;
	block_lba = block_bitmap_alloc(cur_part);
	if (block_lba == -1) {
		printk("fs.c block_bitmap_alloc alloc is failed!!!!!\n");
		rollback_step = 2;
		goto rollback;
	}
	new_dir_inode.i_sectors[0] = block_lba;
	block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
	ASSERT(block_bitmap_idx != 0);
	bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

	ASSERT(cur_thread->stack_magic == 0x19870916);
	struct dir_entry* p_de = (struct dir_entry*)io_buf;
	memcpy(p_de->filename, ".", 1);
	p_de->i_no = inode_no;
	p_de->f_type = FT_DIRECTORY;
	p_de++;
	ASSERT(cur_thread->stack_magic == 0x19870916);
	memcpy(p_de->filename, "..", 2);
	struct dir* parent_dir = searched_record.parent_dir;
	p_de->i_no = parent_dir->inode->i_no;
	p_de->f_type = FT_DIRECTORY;

	ASSERT(cur_thread->stack_magic == 0x19870916);
	new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;
	ide_write(cur_part->my_disk, block_lba, io_buf, 1);

	ASSERT(cur_thread->stack_magic == 0x19870916);
	struct dir_entry new_dir_entry;
	memset(&new_dir_entry, 0, sizeof(struct dir_entry));
	char* dirname = strrchr(searched_record.searched_path, '/') + 1;
	ASSERT(cur_thread->stack_magic == 0x19870916);
	create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);
	ASSERT(cur_thread->stack_magic == 0x19870916);
	memset(io_buf, 0, SECTOR_SIZE * 2);
	if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
		printk("sync_dir_entry for parent dir is failed!!!!\n");
		rollback_step = 2;
		goto rollback;
	}
	ASSERT(cur_thread->stack_magic == 0x19870916);
	memset(io_buf, 0, SECTOR_SIZE * 2);
	//同步父目录的inode
	inode_sync(cur_part, parent_dir->inode, io_buf);
	memset(io_buf, 0, SECTOR_SIZE * 2);
	//同步本目录的inode
	inode_sync(cur_part, &new_dir_inode, io_buf);
	//同步inode_bitmap 
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);
	sys_free(io_buf);
	dir_close(parent_dir);
	return 0;

rollback:
	switch (rollback_step) {
		case 2:
			bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
			break;
		case 1:
			dir_close(searched_record.parent_dir);
			break;
	}
	sys_free(io_buf);
	return -1;
}

struct dir* sys_opendir(const char* name) {
	ASSERT(strlen(name) < MAX_PATH_LEN);
	if (name[0] == '/' && (name[1] == 0 || name[0] == '.')) {
		return &root_dir;
	}
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(name, &searched_record);
	struct dir* ret = NULL;
	if (inode_no == -1) {
		printk("In %s sub path %s not exist\n", name, searched_record.searched_path);
	} else {
		if (searched_record.file_type == FT_REGULAR) {
			printk("%s is regular file!!!\n", name);
		} else if (searched_record.file_type == FT_DIRECTORY) {
			ret = dir_open(cur_part, inode_no);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}


int32_t sys_closedir(struct dir* dir) {
	if (dir == NULL) return -1;
	dir_close(dir);
	return 0;
}

struct dir_entry* sys_readdir(struct dir* dir) {
	ASSERT(dir != NULL);
	return dir_read(dir);
}

void sys_rewinddir(struct dir* dir) {
	dir->dir_pos = 0;
}


static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void* io_buf) {
//	printk("get_parent_dir_inode_nr error111 child_inode_nr: %d\n", child_inode_nr);
	struct inode* child_dir_inode = inode_open(cur_part, child_inode_nr);
	//printk("get_parent_dir_inode_nr error222\n");
	uint32_t block_lba = child_dir_inode->i_sectors[0];
//	printk("block_lba: 0x%x\n", block_lba);
	ASSERT(block_lba >= cur_part->sb->data_start_lba);
	inode_close(child_dir_inode);
	ide_read(cur_part->my_disk, block_lba, io_buf, 1);
	struct dir_entry* dir_e = (struct dir_entry*)io_buf;
	ASSERT(dir_e[1].i_no < 4096 && dir_e[1].f_type == FT_DIRECTORY);

	return dir_e[1].i_no;
	//return 0;
}


static int get_child_dir_name(uint32_t p_inode_nr, uint32_t c_inode_nr, char* path, void* io_buf) {
	struct inode* p_inode = inode_open(cur_part, p_inode_nr);
	uint8_t block_idx = 0;
	uint32_t all_blocks[140] = {0}, block_cnt = 12;
	while (block_idx < 12) {
		all_blocks[block_idx] = p_inode->i_sectors[block_idx];
		block_idx++;
	}
	if (p_inode->i_sectors[12]) {
		ide_read(cur_part->my_disk, p_inode->i_sectors[12], io_buf, 1);
		block_cnt = 140;
	}
	inode_close(p_inode);
	struct dir_entry* dir_e = (struct dir_entry*)io_buf;
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
	uint32_t dir_entrys_sec = SECTOR_SIZE / dir_entry_size;
	block_idx = 0;
	
	while (block_idx < block_cnt) {
		if (all_blocks[block_idx]) {
			ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
			uint8_t dir_e_idx = 0;
			while (dir_e_idx < dir_entrys_sec) {
				if ((dir_e + dir_e_idx)->i_no == c_inode_nr) {
					strcat(path, "/");
					strcat(path, (dir_e + dir_e_idx)->filename);
					return 0;
				}
				dir_e_idx++;
			}
		}
		block_idx++;
	}
	return -1;
}

char* sys_getcwd(char* buf, uint32_t size) {
	ASSERT(buf != NULL);
	void* io_buf = sys_malloc(SECTOR_SIZE);
	//printk("io_bufaddr: 0x%x\n", (uint32_t)io_buf);
	if (io_buf == NULL) {
		return NULL;
	}
	struct task_struct* cur_thread = running_thread();
	int32_t parent_inode_nr = 0;
	int32_t child_inode_nr = cur_thread->cwd_inode_nr;
	//printk("child_inode_nr : %d\n", child_inode_nr);
	ASSERT(cur_thread->cwd_inode_nr >= 0 && cur_thread->cwd_inode_nr < 4096);
	if (child_inode_nr == 0) {
		buf[0] = '/';
		buf[1] = 0;
		sys_free(io_buf);
		return buf;
	}
	memset(buf, 0, size);
	//char full_path_reverse[MAX_PATH_LEN] = {0};
	char* full_path_reverse = (char*)sys_malloc(MAX_PATH_LEN);
	while (child_inode_nr) {
	//	printk("sys_getcwd error111\n");
		parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);
	//	printk("parent_inode_nr: %d\n", parent_inode_nr);
		if (get_child_dir_name(parent_inode_nr, child_inode_nr, full_path_reverse, io_buf) == -1) {
			sys_free(io_buf);
			sys_free(full_path_reverse);
			return NULL;
		}
		child_inode_nr = parent_inode_nr;
	}
	ASSERT(strlen(full_path_reverse) <= size);
	char* last_slash;
	while ((last_slash = strrchr(full_path_reverse, '/'))) {
		uint16_t len = strlen(buf);
		strcpy(buf + len, last_slash);
		*last_slash = 0;
	}
	sys_free(full_path_reverse);
	sys_free(io_buf);
	return buf;
}

//cd 命令 更改工作目录 path 为绝对路径
int32_t sys_chdir(const char* path) {
	uint32_t ret = -1;
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(path, &searched_record);
	printk("cd path: %s inode: %d\n", path, inode_no);
	if (inode_no != -1) {
		if (searched_record.file_type == FT_DIRECTORY) {
			running_thread()->cwd_inode_nr = inode_no;
			ret = 0;
		} else {
			printk("sys_chdir: %s is regular file or other\n", path);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}

int32_t sys_stat(const char* path, struct stat* buf) {
	if (!strcmp(path, "/") || !strcmp(path, "/.") || !strcmp(path, "/..")) {
		buf->st_filetype = FT_DIRECTORY;
		buf->st_no = 0;
		buf->st_size = root_dir.inode->i_size;
		return 0;
	}
	int ret = -1;
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(path, &searched_record);
	if (inode_no != -1) {
		struct inode* obj_inode = inode_open(cur_part, inode_no);
		buf->st_size = obj_inode->i_size;
		inode_close(obj_inode);
		buf->st_filetype = searched_record.file_type;
		buf->st_no = inode_no;
		ret = 0;
	} else {
		printk("sys_stat %s is not found\n", path);
	}
	dir_close(searched_record.parent_dir);
	return ret;
}

void sys_help() {
	printk("buildin commands:\n");
	printk("ls: show directory or file information\n");
	printk("cd: chage current workd directory\n");
	printk("mkdir: create a directory\n");
	printk("rm: remove a regular file\n");
	printk("pwd: show current work directory\n");
	printk("ps: show process information\n");
	printk("clear: clear screen\n");
	printk("shortcut key:\n");
	printk("ctrl + l: clear screen\n");
	printk("ctrl + u: clear input\n\n");
}

/*
struct partition* cur_part;

static bool mount_partition(struct list* list_elem, int arg) {
	char* part_name = (char*)arg;
	struct partition* part = elem2entry(struct partition, part_name, pelem);
	if (!strcmp(part->name, name)) {
		cur_part = part;
		struct disk* hd = cur_part->my_disk;
	//	struct super_block* sb_buf = (struct super_block*)sys_malloc(sizeof(struct super_block));
		cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
		if (cur_part->sb == NULL) PANIC("alloc memory failed!!!");
		memset(cur_part->sb, 0, sizeof(struct super_block));
		ide_read(hd, cur_part->start_lba + 1, cur_part->sb, 1);
		cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
		if (cur_part->block_bitmap.bits == NULL) PANIC("alloc memory failed!!!");
		cur_part->block_bitmap.btmp_bytes_len = cur_part->sb->block_bitmap_sects * SECTOR_SIZE;

		ide_read(hd, cur_part->sb->block_bitmap_lba, cur_part->block_bitmap.bits, cur_part->sb->block_bitmap_sects);

		cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sizeof(struct super_block) * SECTOR_SIZE);
		if (cur_part->inode_bitmap.bits == NULL) PANIC("alloc memory failed!!!!");
		ide_read(hd, cur_part->sb->inode_bitmap_lba, cur_part->inode_bitmap.bits, cur_part->sb->inode_bitmap_sects);
		list_init(&cur_part->open_inodes);
		printkf("mount %s done!\n", part->name);
		return true;
	}
	return false;
}
*/



