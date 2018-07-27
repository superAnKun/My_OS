#ifndef __FS_H__
#define __FS_H__
#include "../lib/stdio.h"
#include "../stdint.h"
#include "however.h"
/*
#define MAX_FILES_PER_PART 4096
#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE
#define MAX_PATH_LEN 512
*/
/*
enum file_types {
	FT_UNKNOWN,  //未知文件类型
	FT_REGULAR,   //普通文件
	FT_DIRECTORY  //目录
};
*/
enum oflags {
	O_RDONLY = 1,
	O_WRONLY = 2,
	O_RDWR = 4,
	O_CREAT = 8
};

enum SEEK {
	SEEK_SET = 1,
	SEEK_CUR = 2,
	SEEK_END = 3
};

struct path_search_record {
	char searched_path[MAX_PATH_LEN];  //查找路径
	struct dir* parent_dir;         //文件父目录
	enum file_types file_type;      //文件类型
};


//描述文件属性的结构体
struct stat {
	uint32_t st_no;     //文件inode号
	uint32_t st_size;    //文件大侠
	enum file_types st_filetype; //文件类型
};

void filesys_init();
int32_t sys_open(const char* pathname, uint8_t flags);
int32_t sys_write(int32_t fd, const void* buf, uint32_t count);
int32_t sys_unlink(const char* pathname);
int32_t sys_read(int32_t fd, void* buf, uint32_t count);
char* sys_getcwd(char* buf, uint32_t size);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
uint32_t sys_mkdir(const char* filename);
struct dir_entry* sys_readdir(struct dir* dir);
void sys_rewinddir(struct dir* dir);
int32_t sys_stat(const char* path, struct stat* buf);
void sys_ps();
int32_t sys_rmdir(const char* pathname);
struct dir* sys_opendir(const char* name);
int32_t sys_closedir(struct dir* dir);
int32_t sys_chdir(const char* path);
int32_t sys_close(int32_t fd);
void sys_help();
extern struct partition* cur_part;
#endif
