#ifndef _HOWEVER_H_
#define _HOWEVER_H_
#include "../stdint.h"
#define MAX_FILES_PER_PART 4096
#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE
#define MAX_PATH_LEN 512
enum file_types {
	FT_UNKNOWN,  //未知文件类型
	FT_REGULAR,   //普通文件
	FT_DIRECTORY, //目录
};

bool is_pipe(uint32_t local_fd);
int32_t sys_pipe(int32_t pipefd[2]);
uint32_t pipe_read(int32_t fd, void* buf, uint32_t count);
uint32_t pipe_write(int32_t fd, void* buf, uint32_t count);
void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd);
#endif
