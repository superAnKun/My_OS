#include "init.h"
#include "print.h"
#include "debug.h"
#include "../device/ioqueue.h"
#include "../device/keyboard.h"
#include "../userprog/process.h"
#include "../user/syscall.h"
#include "../lib/stdio.h"
#include "../shell/shell.h"
void k_thread_a(void*);
void k_thread_b(void*);
void k_thread_c(void*);
void u_prog_a();
void u_prog_b();
int test_var_a = 0;
int test_var_b = 0;
/*
void printInfo(char* filename) {
	struct dir* p_dir = sys_opendir(filename);
	if (p_dir) {
		printf("%s open done!\n content: \n", filename);
		char* type = NULL;
		struct dir_entry* dir_e = NULL;
		while ((dir_e = sys_readdir(p_dir))) {
			type = "NUKOUN";
			if (dir_e->f_type == FT_REGULAR) {
				type = "regular";
			} else {
				type = "directory";
			}
			printf("	%s    %s\n", type, dir_e->filename);
		}
		if (sys_closedir(p_dir) == 0) {
			printf("%s close done\n", filename);
		} else {
			printf("%s close failed!!!\n", filename);
		}
	} else {
		printf("%s open failed!!!\n", filename);
	}
}
*/
void process_execute(void* filename, char* name);
void printInfo(char* filename);
void init();
char buf[2000] = {0};
void main()
{
	put_str("I am kernel\n");
	init_all();
	//struct task_struct* cur_thread = running_thread();
	//thread_start("K_thread_b", 31, k_thread_b, "argB ");
	//thread_start("k_thread_a", 31, k_thread_a, "argA ");
	//process_execute(u_prog_a, "user_prog_a");
	//process_execute(u_prog_b, "user_prog_b");
//	process_execute(init, "init");
//    sys_mkdir("/dir2");
	//if (sys_rmdir("/dir2/subdir3") == 0) {
	//	printf("detete /dir2/subdir2/ done!!\n");
	//}
//printInfo("/dir3");
//	clear();

	uint32_t file_size =  6792;
	uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
	struct disk* sda = &channels[0].devices[0];
	void* prog_buf = sys_malloc(file_size);
	ide_read(sda, 300, prog_buf, sec_cnt);
	int32_t fd = sys_open("/a", O_CREAT | O_RDWR);
	if (fd != -1) {
		if (sys_write(fd, prog_buf, file_size) == -1) {
			printk("file write error!!!\n");
			while (1);
		}
	}
	sys_close(fd);
	console_put_str("[rabbit@localhost /]$ ");
	intr_enable();
	while(1);
}
/*
void init() {
	int32_t ret_pid = fork();
	ret_pid = fork();
	if (ret_pid) {
		printf("I am father, my pid is %d, child pid is 0x%x\n", getpid(), ret_pid);
	} else {
		printf("I am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid);
	}
	while(1);
}
*/
void u_prog_a() {
	void* addr1 = malloc(256);
	printf("addr1: 0x%x\n", addr1);
	free(addr1);
	while (1);
}

void u_prog_b() {
	while (1);
}

void k_thread_a(void *arg) {
	char *str = (char*)arg;
	while (1) {
		console_put_str(" v_a:0x:");
		void* addr = sys_malloc(33);
		console_put_int((uint32_t)addr);
		console_put_str("\n");
		while (1);
	}
}

void k_thread_b(void *arg) {
	char *str = (char*)arg;
	while (1) {
		console_put_str(" v_b:0x:");
		void* addr = sys_malloc(63);
		console_put_int((uint32_t)addr);
		console_put_str("\n");
		while(1);
	}
}
void k_thread_c(void *arg) {
	while (1) {
		console_put_str("thread_c+++++++++++++++++++++++++++++++++++++++++++++++\n");
	}
}



void printInfo(char* filename) {
	struct dir* p_dir = sys_opendir(filename);
	if (p_dir) {
		printf("%s open done!\n content: \n", filename);
		char* type = NULL;
		struct dir_entry* dir_e = NULL;
		while ((dir_e = sys_readdir(p_dir))) {
			type = "NUKOUN";
			if (dir_e->f_type == FT_REGULAR) {
				type = "regular";
			} else {
				type = "directory";
			}
			printf("	%s    %s		aaaa::%d\n", type, dir_e->filename, dir_e->i_no);
		}
		if (sys_closedir(p_dir) == 0) {
			printf("%s close done\n", filename);
		} else {
			printf("%s close failed!!!\n", filename);
		}
	} else {
		printf("%s open failed!!!\n", filename);
	}
}

