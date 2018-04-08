#include "sync.h"
#include "thread.h"
//初始化信号量
void sema_init(struct semaphore *psema, uint8_t value) {
	psema->value = value;
	list_init(&psema->waiters);
}

void lock_init(struct lock* plock, uint8_t value) {
	plock->holder = NULL;
	sema_init(&plock->semaphore, value);
	plock->holder_repeat_nr = 0;
}

void sema_down(struct semaphore *psema) {
	enum intr_status old_status  = intr_disable();
	while (psema->value == 0) {
		if (!elem_find(&psema->waiters, &(running_thread()->general_tag))) {
			list_append(&psema->waiters, &(running_thread()->general_tag));
		}
		thread_block(TASK_BLOCKED);
	}
	psema->value--;
	ASSERT(psema->value == 0);   //强制二元锁
	intr_set_status(old_status);
}

void sema_up(struct semaphore *psema) {
	enum intr_status old_status = intr_disable();
	ASSERT(psema->value == 0);   //强制二元锁
	if (!list_empty(&psema->waiters)) {
		struct task_struct *pthread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
		thread_unblock(pthread_blocked);
	}
	psema->value++;
	ASSERT(psema->value == 1);
	intr_set_status(old_status);
}

void lock_acquire(struct lock* plock) {
	if (plock->holder != running_thread()) {
		sema_down(&plock->semaphore);
		plock->holder = running_thread();
		ASSERT(plock->holder_repeat_nr == 0);
		plock->holder_repeat_nr = 1;
	} else {
		plock->holder_repeat_nr++;
	}
}

void lock_release(struct lock* plock) {
	ASSERT(plock->holder == running_thread());
	if (plock->holder != running_thread()) return;
	if (plock->holder_repeat_nr > 1) {
		plock->holder_repeat_nr--;
		return;
	}
	ASSERT(plock->holder_repeat_nr == 1);
	plock->holder = NULL;
	plock->holder_repeat_nr = 0;
	sema_up(&plock->semaphore); //信号量的V操作 
}


