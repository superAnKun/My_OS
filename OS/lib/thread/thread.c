#include "thread.h"
#include "../stdint.h"
#include "../kernel/string.h"
#include "../kernel/global.h"
#include "../kernel/memory.h"
#include "../kernel/interrupt.h"
#include "../kernel/list.h"
#include "../userprog/process.h"
#include "../shell/shell.h"

struct task_struct *main_thread;
struct list thread_ready_list;
struct list thread_all_list;
static struct list_elem *thread_tag;  //保存队列中线程结点
struct task_struct* idle_thread;


uint8_t pid_bitmap_bits[128] = {0};
struct pid_pool {
	struct bitmap pid_bitmap;
	uint32_t pid_start;
	struct lock pid_lock;
};

struct pid_pool pid_pool;

extern void switch_to(struct task_struct *cur, struct task_struct *next);

struct task_struct* running_thread() {
	uint32_t esp;
	asm ("mov %%esp, %0" : "=g" (esp));
	return (struct task_struct*) (esp & 0xfffff000);
}


static void kernel_thread(thread_func *function, void *func_arg) {
	intr_enable();     //开始调用线程后打开中断
	function(func_arg);
}

static void pid_pool_init() {
	pid_pool.pid_start = 1;
	pid_pool.pid_bitmap.bits = pid_bitmap_bits;
	pid_pool.pid_bitmap.btmp_bytes_len = 128;
	bitmap_init(&pid_pool.pid_bitmap);
	lock_init(&pid_pool.pid_lock);
}

static pid_t allocate_pid() {
	lock_acquire(&pid_pool.pid_lock);
	int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
	bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
	lock_release(&pid_pool.pid_lock);
	return pid_pool.pid_start + bit_idx;
}

static void release_pid(pid_t pid) {
	lock_acquire(&pid_pool.pid_lock);
	int32_t bit_idx = pid - pid_pool.pid_start;
	bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
	lock_release(&pid_pool.pid_lock);
}

/*
struct lock pid_lock;
static pid_t allocate_pid() {
	static pid_t next_pid = 0;
	lock_acquire(&pid_lock);
	next_pid++;
	lock_release(&pid_lock);
	return next_pid;
}
*/
pid_t fork_pid() {
	return allocate_pid();
}

void init_thread(struct task_struct *pthread, char *name, int prio) {
	memset(pthread, 0, sizeof(*pthread));
	pthread->pid = allocate_pid();
	strcpy(pthread->name, name);
	if (pthread == main_thread) {
		pthread->status = TASK_RUNNING;
	} else{
		pthread->status = TASK_READY;
	}
	pthread->priority = prio;
	pthread->ticks = prio;
	pthread->elapsed_ticks = 0;
	pthread->pgdir = NULL;
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);


	pthread->fd_table[0] = 0;
	pthread->fd_table[1] = 1;
	pthread->fd_table[2] = 2;
	for (uint8_t i = 3; i < MAX_FILES_OPEN_PER_PROC; i++) {
		pthread->fd_table[i] = -1;
	}

	pthread->parent_pid = -1;
	pthread->cwd_inode_nr = 0;  //进程所在inode编号 以根目录为默认工作目录
	pthread->stack_magic = 0x19870916;
}

void thread_create(struct task_struct* pthread, thread_func function, void *func_arg) {
	//pthread->self_kstack -= sizeof(struct intr_stack);
	//pthread->self_kstack -= sizeof(struct thread_stack);
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread->self_kstack - sizeof(struct intr_stack));
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread->self_kstack - sizeof(struct thread_stack));

	struct thread_stack *kthread_stack = (struct thread_stack*)pthread->self_kstack;
	kthread_stack->ebp = kthread_stack->ebp = 0;
	kthread_stack->ebx = kthread_stack->ebx = 0;
	kthread_stack->esi = kthread_stack->esi = 0;
	kthread_stack->edi = kthread_stack->edi = 0;
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->eip = kernel_thread;
}

struct task_struct *thread_start(char *name, int prio, thread_func function, void *func_arg) {
	struct task_struct* thread = get_kernel_pages(1);
	init_thread(thread, name, prio);
	//***************万不得已*************************
	thread->cwd_inode_nr = 0;  //进程所在inode编号 以根目录为默认工作目录
	thread->parent_pid = -1;
	//thread->stack_magic = 0x19870916;
	//************************************************
	thread_create(thread, function, func_arg);
	ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
	list_append(&thread_ready_list, &thread->general_tag);


	ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
	list_append(&thread_all_list, &thread->all_list_tag);
//	asm volatile ("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%esi; pop %%edi; ret": :"g" (thread->self_kstack) : "memory");
	return thread;
}


static void make_main_thread(void) {
	main_thread = running_thread();
	init_thread(main_thread, "main", 8);
	ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}


void schedule() {
	ASSERT(intr_get_status() == INTR_OFF);

	struct task_struct *cur = running_thread();
	//正常时间片用完退出
	if (cur->status == TASK_RUNNING) {
		ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
		list_append(&thread_ready_list, &cur->general_tag);
		cur->ticks = cur->priority;
		cur->status = TASK_READY;
	} else {
    //使用锁等情况 把general_tag加入到另外一个队列中 所以这里不需要把当前线程加入就绪队列
	}

	//就绪队列中没有可运行的任务时就唤醒idle
	if (list_empty(&thread_ready_list)) {
		thread_unblock(idle_thread);
	}
	ASSERT(!list_empty(&thread_ready_list));
	thread_tag = NULL;
	thread_tag = list_pop(&thread_ready_list);
	struct task_struct *next = elem2entry(struct task_struct, general_tag, thread_tag);
	next->status = TASK_RUNNING;
	process_activate(next);
	switch_to(cur, next);
}

void thread_block(enum task_status stat) {
	ASSERT((stat == TASK_BLOCKED || stat == TASK_WAITING || stat == TASK_HANGING));
	enum intr_status old_status = intr_disable();
	struct task_struct *cur_thread = running_thread();
	cur_thread->status = stat;
	schedule();
	intr_set_status(old_status);
}

void thread_unblock(struct task_struct *pthread) {
	enum intr_status old_status = intr_disable();
	ASSERT((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING));
	if (pthread->status != TASK_READY) {
		//ASSERT(&pthread_ready_list, &pthread->general_tag);
	    if (elem_find(&thread_ready_list, &pthread->general_tag)) {
			PANIC("thread_unblock: blocked thread in ready_list\n");
			intr_set_status(old_status);
			return;
		}
		pthread->status = TASK_READY;

		//加到等待队列的队首
		list_push(&thread_ready_list, &pthread->general_tag);
	}
	intr_set_status(old_status);
}

void idle() {
	while (1) {
		//阻塞自己
		thread_block(TASK_BLOCKED);
		asm volatile ("sti; hlt" ::: "memory");
	}
}

void thread_yield() {
	struct task_struct* cur = running_thread();
	enum intr_status old_status = intr_disable();
	ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
	list_append(&thread_ready_list, &cur->general_tag);
	cur->status = TASK_READY;
	schedule();
	intr_set_status(old_status);
}

void init() {
	int32_t ret_pid = fork();
	//ret_pid = fork();
	if (ret_pid) {
		printf("I am father, my pid is %d, child pid is 0x%x\n", getpid(), ret_pid);
	} else {
		my_shell();
	}
	while(1);
}
/*
void thread_init() {
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	lock_init(&pid_lock, 1);
	process_execute(init, "init");
	make_main_thread();
	idle_thread = thread_start("idle", 10, idle, NULL);
	put_str("thread_init done\n");
}
*/
void thread_exit(struct task_struct* thread_over, bool need_schedule) {
	intr_disable();
	thread_over->status = TASK_DIED;

	if (elem_find(&thread_ready_list, &thread_over->general_tag)) {
		list_remove(&thread_over->general_tag);
	}
	list_remove(&thread_over->all_list_tag);
	if (thread_over->pgdir) {
		mfree_page(PF_KERNEL, thread_over->pgdir, 1);
	}

	release_pid(thread_over->pid);
	if (thread_over != main_thread) {
	    mfree_page(PF_KERNEL, thread_over, 1);
	}

	if (need_schedule) {
		schedule();
		PANIC("thread_exit: should not be here\n");
	}
}

static bool pid_check(struct list_elem* pelem, int32_t pid) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	if (pthread->pid == pid) return true;
	return false;
}

struct task_struct* pid2thread(int32_t pid) {
	struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
	if (pelem == NULL) return NULL;
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	return pthread;
}


void thread_init() {
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	pid_pool_init();
	//lock_init(&pid_lock, 1);
	process_execute(init, "init");
	make_main_thread();
	idle_thread = thread_start("idle", 10, idle, NULL);
	put_str("thread_init done\n");
}


static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {
	memset(buf, 0, sizeof(buf));
	uint8_t out_pad_0idx = 0;
	switch (format) {
		case 's':
			out_pad_0idx = sprintf(buf, "%s", ptr);
			break;
		case 'd':
			out_pad_0idx = sprintf(buf, "%d", *((uint16_t*)ptr));
			break;
		case 'x':
			out_pad_0idx = sprintf(buf, "%x", *((uint32_t*)ptr));
			break;
	}
	while (out_pad_0idx < buf_len) {
		buf[out_pad_0idx] = ' ';
		out_pad_0idx++;
	}
	sys_write(stdout_no, buf, buf_len - 1);
}

static bool elem2thread_info(struct list_elem* pelem, int arg) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	char out_pad[16] = {0};
	pad_print(out_pad, 16, &pthread->pid, 'd');
	if (pthread->parent_pid == -1) {
		pad_print(out_pad, 16, "NULL", 's');
	} else {
		pad_print(out_pad, 16, &pthread->parent_pid, 'd');
	}
	switch (pthread->status) {
		case 0:
			pad_print(out_pad, 16, "RUNNING", 's');
			break;
		case 1:
			pad_print(out_pad, 16, "READY", 's');
			break;
		case 2:
			pad_print(out_pad, 16, "BLOCKED", 's');
			break;
		case 3:
			pad_print(out_pad, 16, "WAITING", 's');
			break;
		case 4:
			pad_print(out_pad, 16, "HANGING", 's');
			break;
		case 5:
			pad_print(out_pad, 16, "DIED", 's');
			break;
	}
	pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

	memset(out_pad, 0, 16);
	ASSERT(strlen(pthread->name) < 17);
	memcpy(out_pad, pthread->name, strlen(pthread->name));
	strcat(out_pad, "\n");
	sys_write(stdout_no, out_pad, strlen(out_pad));
	return false;
}

void sys_ps() {
	char* ps_title = "PID             PPID            STAT            TICKS           COMMAND\n";
	sys_write(stdout_no, ps_title, strlen(ps_title));
	list_traversal(&thread_all_list, elem2thread_info, 0);
}
