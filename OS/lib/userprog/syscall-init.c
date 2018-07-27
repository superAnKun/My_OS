#include "../user/syscall.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "../kernel/string.h"
#include "../kernel/memory.h"
#include "process.h"
#include "../shell/shell.h"
/*
 以下放入 ../kernel/下的syscall-init.h中
 */

#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

uint32_t sys_getpid() {
	return running_thread()->pid;
}

/*
uint32_t sys_write(char* str) {
	console_put_str(str);
	return strlen(str);
}
*/

void syscall_init() {
	put_str("syscall_init start\n");
	syscall_table[SYS_GETPID] = sys_getpid;
	syscall_table[SYS_WRITE] = sys_write;
	syscall_table[SYS_MALLOC] = sys_malloc;
	syscall_table[SYS_FREE] = sys_free;
	syscall_table[SYS_FORK] = sys_fork;
	syscall_table[SYS_READ] = sys_read;
	syscall_table[SYS_CLEAR] = cls_screen;
	syscall_table[SYS_PUTCHAR] = console_put_char;
	syscall_table[SYS_GETCWD] = sys_getcwd;
	syscall_table[SYS_OPEN] = sys_open;
	syscall_table[SYS_CLOSE] = sys_close;
	syscall_table[SYS_LSEEK] = sys_lseek;
	syscall_table[SYS_UNLINK] = sys_unlink;
	syscall_table[SYS_MKDIR] = sys_mkdir;
	syscall_table[SYS_OPENDIR] = sys_opendir;
	syscall_table[SYS_CLOSEDIR] = sys_closedir;
	syscall_table[SYS_CHDIR] = sys_chdir;
	syscall_table[SYS_RMDIR] = sys_rmdir;
	syscall_table[SYS_READDIR] = sys_readdir;
	syscall_table[SYS_REWINDDIR] = sys_rewinddir;
	syscall_table[SYS_STAT] = sys_stat;
	syscall_table[SYS_PS] = sys_ps;
	syscall_table[SYS_EXECV] = sys_execv;
	syscall_table[SYS_EXIT] = sys_exit;
	syscall_table[SYS_WAIT] = sys_wait;
	syscall_table[SYS_PIPE] = sys_pipe;
	syscall_table[SYS_FD_REDIRECT] = sys_fd_redirect;
	syscall_table[SYS_HELP] = sys_help;
	put_str("syscall_init done\n");
}
