#ifndef __FILE_H_
#define __FILE_H_
#include "../stdint.h"
struct file {
	uint32_t pos;
	uint32_t fd_flag;
	struct inode* fd_inode;
};

enum std_d {
	stdin_no,
	stdout_no,
	stderr_no
};

enum bitmap_type {
	INODE_BITMAP,
	BLOCK_BITMAP
};

//系统打开的最大文件数
#define MAX_FILE_OPEN 32

extern struct file file_table[MAX_FILE_OPEN];

uint32_t fd_locall2global(uint32_t local_fd);
#endif
