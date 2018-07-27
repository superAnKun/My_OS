#ifndef __SHELL_H_
#define __SHELL_H_
#include "../stdint.h"
#include "../user/syscall.h"
#include "../fs/file.h"
#include "../kernel/debug.h"
char final_path[MAX_PATH_LEN];
void my_shell();
void make_clear_abs_path(char* path, char* final_path);
/*
bool is_pipe(uint32_t local_fd);
int32_t sys_pipe(int32_t pipefd[2]);
uint32_t pipe_read(int32_t fd, void* buf, uint32_t count);
uint32_t pipe_write(int32_t fd, void* buf, uint32_t count);
*/
#endif
