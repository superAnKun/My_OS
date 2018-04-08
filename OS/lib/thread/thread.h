#ifndef _THREAD_H
#define _THREAD_H
#include "../stdint.h"
#include "../kernel/list.h"
typedef void thread_func(void*);

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
	struct list_elem all_list_tag;
	uint32_t *pgdir;                     //进程自己页表的虚拟地址
	uint32_t stack_magic;  //边界标记 防止栈溢出
};


#endif
