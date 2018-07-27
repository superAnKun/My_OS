#include "ioqueue.h"

void ioqueue_init(struct ioqueue* ioq) {
	lock_init(&ioq->lock, 1);
	ioq->producer = ioq->consumer = NULL;
	ioq->head = ioq->tail = 0;
}

bool ioq_empty(struct ioqueue* ioq) {
	ASSERT(intr_get_status() == INTR_OFF);
	return ioq->head == ioq->tail;
}

bool ioq_full(struct ioqueue* ioq) {
	return (ioq->head + 1) % bufsize == ioq->tail;
}

void ioq_wait(struct task_struct** waiter) {
	ASSERT(*waiter == NULL && waiter != NULL);
	*waiter = running_thread();
	thread_block(TASK_BLOCKED);
}

void ioq_wakeup(struct task_struct** waiter) {
	ASSERT(waiter != NULL && *waiter != NULL);
	thread_unblock(*waiter);
	*waiter = NULL;
}

char ioq_getchar(struct ioqueue* ioq) {
	ASSERT(intr_get_status() == INTR_OFF);
	while (ioq_empty(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);
		lock_release(&ioq->lock);
	}
	char byte = ioq->buffer[ioq->tail];
	//++ioq->tail %= bufsize;
	ioq->tail++;
	ioq->tail %= bufsize;
	if (ioq->producer != NULL) {
		ioq_wakeup(&ioq->producer);
	}
	return byte;
}

void ioq_putchar(struct ioqueue* ioq, char byte) {
	ASSERT(intr_get_status() == INTR_OFF);
	while (ioq_full(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->producer);
		lock_release(&ioq->lock);
	}
	ioq->buffer[ioq->head] = byte;
	//++ioq->head %= bufsize;
	ioq->head++;
	ioq->head %= bufsize;
	if (ioq->consumer != NULL) {
		ioq_wakeup(&ioq->consumer);
	}
}

uint32_t ioq_length(struct ioqueue* ioq) {
	uint32_t len = 0;
	if (ioq->head >= ioq->tail) {
		len = ioq->head - ioq->tail;
	} else {
		len = bufsize - (ioq->tail - ioq->head);
	}
	return len;
}
