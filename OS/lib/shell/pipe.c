#include "../fs/however.h"
#include "../stdint.h"
#include "shell.h"
#include "../device/ioqueue.h"
#define PIPE_FLAG 0xFF
bool is_pipe(uint32_t local_fd) {
	uint32_t global_fd = fd_locall2global(local_fd);
	return (file_table[global_fd].fd_flag == 0xffff);
}

int32_t sys_pipe(int32_t pipefd[2]) {
	int32_t global_fd = get_free_slot_in_global();
	file_table[global_fd].fd_inode = get_kernel_pages(1);
	if (file_table[global_fd].fd_inode == NULL) return -1;

	ioqueue_init((struct ioqueue*)file_table[global_fd].fd_inode);

	file_table[global_fd].fd_flag = 0xffff;
	file_table[global_fd].pos = 2;   //fd_pos 复用为管道打开的个数
	pipefd[0] = pcb_fd_install(global_fd);
	pipefd[1] = pcb_fd_install(global_fd);
	return 0;
}

uint32_t pipe_read(int32_t fd, void* buf, uint32_t count) {
	char* buffer = buf;
	uint32_t global_fd = fd_locall2global(fd);
	struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;
	uint32_t ioq_len = ioq_length(ioq);
	uint32_t size = ioq_len > count ? count : ioq_len;
	uint32_t cnt = 0;
	while (cnt < size) {
		*buffer = ioq_getchar(ioq);
		//printk("read: %c\n", *buffer);
		cnt++;
		buffer++;
	}
	return cnt;
}

uint32_t pipe_write(int32_t fd, void* buf, uint32_t count) {
	uint32_t global_fd = fd_locall2global(fd);
	struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;
	uint32_t ioq_left = bufsize - ioq_length(ioq);
	uint32_t size = ioq_left > count ? count : ioq_left;
	const char* buffer = buf;
	uint32_t cnt = 0;
	while (cnt < size) {
		ioq_putchar(ioq, *buffer);
		//printk("write: %c\n", *buffer);
		cnt++;
		buffer++;
	}
	return cnt;
}


void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd) {
	struct task_struct* cur = running_thread();
	if (new_local_fd < 3) {
		cur->fd_table[old_local_fd] = new_local_fd;
	} else {
		uint32_t new_global_fd = cur->fd_table[new_local_fd];
		cur->fd_table[old_local_fd] = new_global_fd;
	}
}


