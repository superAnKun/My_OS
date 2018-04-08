#ifndef _THREAD_SYNC_H_
#define _THREAD_SYNC_H_
#include "list.h"
#include "thread.h"
#include "../stdint.h"
#include "../kernel/interrupt.h"
#include "../kernel/print.h"
#include "../kernel/debug.h"
struct semaphore {
	uint8_t value;
	struct list waiters;
};

struct lock {
	struct task_struct *holder;  //用二元信号量实现锁 此变量说明了锁的持有者
	struct semaphore semaphore; 
	uint32_t holder_repeat_nr;
};

#endif
