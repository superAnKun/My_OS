#include "syscall.h"
#include "../userprog/process.h"
#define _syscall0(NUMBER) ({ \
		uint32_t ret = 0;   \
		asm volatile("int $0x80" : "=a"(ret) : "a"(NUMBER) : "memory"); \
		ret; \
})

#define _syscall1(NUMBER, arg1) ({ \
	uint32_t ret;  \
	asm volatile ("int $0x80" : "=a"(ret) : "a"(NUMBER), "b"(arg1) : "memory"); \
	ret; \
})


#define _syscall2(NUMBER, arg1, arg2) ({ \
	uint32_t ret;  \
	asm volatile ("int $0x80" : "=a"(ret) : "a"(NUMBER), "b"(arg1), "c"(arg2) : "memory"); \
	ret; \
})

#define _syscall3(NUMBER, arg1, arg2, arg3) ({ \
    uint32_t ret;  \
	asm volatile ("int $0x80" : "=a"(ret) : "a"(NUMBER), "b"(arg1), "c"(arg2), "d"(arg3) : "memory"); \
	ret; \
})

//int32_t write(int32_t fd, const void* buf, uint32_t count);

void help() {
	_syscall0(SYS_HELP);
}

uint32_t getpid() {
	//write("hello\n");
	return (uint32_t)_syscall0(SYS_GETPID);
}

int32_t fork() {
	return (int32_t)_syscall0(SYS_FORK);
}

void clear() {
	_syscall0(SYS_CLEAR);
}

void ps() {
	_syscall0(SYS_PS);
}

int32_t pipe(int32_t* fd) {
	return _syscall1(SYS_PIPE, fd);
}

void rewinddir(struct dir* adir) {
	_syscall1(SYS_REWINDDIR, adir);
}

int32_t unlink(const char* filename) {
	return _syscall1(SYS_UNLINK, filename);
}

int32_t mkdir(const char* pathname) {
	return _syscall1(SYS_MKDIR, pathname);
}

struct dir* opendir(const char* name) {
	return (struct dir*)_syscall1(SYS_OPENDIR, name);
}

int32_t closedir(struct dir* dir) {
	return _syscall1(SYS_CLOSEDIR, dir);
}

int32_t rmdir(const char* pathname) {
	return _syscall1(SYS_RMDIR, pathname);
}

struct dir_entry* readdir(struct dir* dir) {
	return (struct dir_entry*)_syscall1(SYS_READDIR, dir);
}

int32_t close(int32_t fd) {
	return _syscall1(SYS_CLOSE, fd);
}

void* malloc(uint32_t size) {
	return _syscall1(SYS_MALLOC, size);
}

void free(void* addr) {
	_syscall1(SYS_FREE, addr);
}

void putchar(char char_asci) {
	_syscall1(SYS_PUTCHAR, char_asci);
}

int32_t chdir(const char* path) {
	return _syscall1(SYS_CHDIR, path);
}

int32_t wait(int32_t* status) {
	return _syscall1(SYS_WAIT, status);
}

int32_t exit(int32_t status) {
	return _syscall1(SYS_EXIT, status);
}

int32_t stat(const char* path, struct stat* buf) {
	return _syscall2(SYS_STAT, path, buf);
}

char* getcwd(char* buf, uint32_t size) {
	return (char*)_syscall2(SYS_GETCWD, buf, size);
}

int32_t open(char* pathname, uint8_t flag) {
	return _syscall2(SYS_OPEN, pathname, flag);
}

void fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd) {
	_syscall2(SYS_FD_REDIRECT, old_local_fd, new_local_fd);
}

int32_t execv(const char* path, const char* argv[]) {
	return _syscall2(SYS_EXECV, path, argv);
}

int32_t read(int32_t fd, void* buf, uint32_t count) {
	return _syscall3(SYS_READ, fd, buf, count);
}

int32_t write(int32_t fd, const void* buf, uint32_t count) {
	return _syscall3(SYS_WRITE, fd, buf, count);
}

int32_t lseek(int32_t fd, int32_t offset, uint8_t whence) {
	return _syscall3(SYS_LSEEK, fd, offset, whence);
}

