#ifndef _THREAD_H
#define _THREAD_H
#include "../stdint.h"
#include "../kernel/list.h"
#include "../kernel/memory.h"
#include "../lib/stdio.h"
#include "sync.h"
#include "../fs/fs.h"
#define PG_SIZE 4096
#define MAX_FILES_OPEN_PER_PROC 32
typedef void thread_func(void*);


extern struct task_struct *main_thread;
extern struct list thread_ready_list;
extern struct list thread_all_list;
//extern struct list_elem *thread_tag;  //保存队列中线程结点
extern struct task_struct *idle_thread;


/*
struct task_struct *main_thread;
struct list thread_ready_list;
struct list thread_all_list;
static struct list_elem *thread_tag;  //保存队列中线程结点
struct task_struct* idle_thread;
*/


struct task_struct *running_thread();

enum task_status {
	TASK_RUNNING,
	TASK_READY,
	TASK_BLOCKED,
	TASK_WAITING,
	TASK_HANGING,
	TASK_DIED
};

//中断压栈结构
struct intr_stack {
	uint32_t vec_no;    //中断号
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp_dummy;

	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;

	uint32_t err_code;
	void (*eip) (void);
	uint32_t cs;
	uint32_t eflags;
	void *esp;
	uint32_t ss;
};


//线程压栈结构 线程函数调用后把此结构压如PCB的栈中
struct thread_stack {
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;

	void (*eip) (thread_func *func, void *func_arg);

	void* unused_retaddr;
	thread_func* function;
	void *func_arg;
};

typedef int16_t pid_t;
//PCB
struct task_struct {
	uint32_t *self_kstack;
	enum task_status status;
	//uint8_t priority;
	char name[16];
	uint8_t priority;
	uint8_t ticks;          //每次上CPU执行的滴答数
	uint32_t elapsed_ticks;     //任务自上CPU后至今占了多少CPU滴答数
	struct list_elem general_tag;
	//uint32_t fd_table[MAX_FILES_OPEN_PER_PROC]; //文件描述符数组
	struct list_elem all_list_tag;
	uint32_t *pgdir;                     //进程自己页表的虚拟地址
	struct virtual_addr userprog_vaddr;    //用户进程的虚拟地址 用户进程的位图
	pid_t pid;
	struct mem_block_desc u_block_desc[DESC_CNT];
	uint32_t fd_table[MAX_FILES_OPEN_PER_PROC]; //文件描述符数组
	uint32_t cwd_inode_nr;   //进程所在目录的inode 编号
	pid_t parent_pid;   //父进程pid 没有父进程 值为 -1
	int8_t exit_status;   //进程结束时自己调用exit传入的参数
	uint32_t stack_magic;  //边界标记 防止栈溢出
	//uint32_t stack_magic;
};//__attribute__((packed));

pid_t sys_fork();
void thread_exit(struct task_struct* thread_over, bool need_schedule);
pid_t sys_wait(int32_t* status);
void sys_exit(int32_t status);
#endif
