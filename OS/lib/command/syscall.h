#ifndef __SYSCALL_H_
#define __SYSCALL_H_
#include "../stdint.h"
#include "../fs/dir.h"
enum SYSCALL_NR {
	SYS_GETPID,
	SYS_WRITE,
	SYS_MALLOC,
	SYS_FREE,
	SYS_FORK,
	SYS_READ,
	SYS_CLEAR,
	SYS_PUTCHAR,
	SYS_GETCWD,
	SYS_OPEN,
	SYS_CLOSE,
	SYS_LSEEK,
	SYS_UNLINK,
	SYS_MKDIR,
	SYS_OPENDIR,
	SYS_CLOSEDIR,
	SYS_CHDIR,
	SYS_RMDIR,
	SYS_READDIR,
	SYS_REWINDDIR,
	SYS_STAT,
	SYS_PS,
	SYS_EXECV,
	SYS_EXIT,
	SYS_WAIT,
	SYS_PIPE,
	SYS_FD_REDIRECT,
	SYS_HELP
};
uint32_t getpid();
int32_t fork();
int32_t write(int32_t fd, const void* buf, uint32_t count);
void* malloc(uint32_t size);
void free(void* addr);
void putchar(char);
void clear();
int32_t read(int32_t fd, void* buf, uint32_t count);
void ps();
void rewinddir(struct dir* dir);
int32_t unlink(const char* filename);
int32_t mkdir(const char* pathname);
struct dir* opendir(const char* name);
int32_t closedir(struct dir* dir);
int32_t rmdir(const char* pathname);
struct dir_entry* readdir(struct dir* dir);
int32_t close(int32_t fd);
int32_t chdir(const char* path);
int32_t stat(const char* path, struct stat* buf);
char* getcwd(char* buf, uint32_t size);
int32_t open(char* pathname, uint8_t flag);
int32_t execv(const char* path, const char* argv[]);
int32_t wait(int32_t*);
int32_t exit(int32_t);
int32_t pipe(int32_t* fd);
void fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd);
void help();
#endif
