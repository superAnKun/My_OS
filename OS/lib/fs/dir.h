#ifndef __DIR_H_
#define __DIR_H_
#include "fs.h"
#include "../stdint.h"
#include "inode.h"
#include "however.h"

#define MAX_FILE_NAME_LEN 16

/*
#define SECTOR_SIZE 512

enum file_typess {
	FT_UNKNOWN,  //未知文件类型
	FT_REGULAR,   //普通文件
	FT_DIRECTORY  //目录
};

*/
struct dir {
	struct inode* inode;
	uint32_t dir_pos;        //记录在目录内的偏移
	uint8_t dir_buf[SECTOR_SIZE];    //目录的数据缓存
};

struct dir_entry {
	char filename[MAX_FILE_NAME_LEN];    //普通文件的或目录名
	uint32_t i_no; //普通文件或目录的inode号
	enum file_types f_type;  //文件类型
};
extern struct dir root_dir;
char* path_parse(char* pathname, char* name_store);
int32_t path_depth_cnt(char* pathname);
#endif
