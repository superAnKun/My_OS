#ifndef _DEVICE_IOQUEUE_H_
#define _DEVICE_IOQUEUE_H_
#include  "../stdint.h"
#include  "../thread/thread.h"
#include  "../thread/sync.h"
#include "../kernel/interrupt.h"
#include "../kernel/global.h"
#include "../kernel/debug.h"


#define bufsize 1024
struct ioqueue {
	struct lock lock;
	struct task_struct* producer;
	struct task_struct* consumer;
	char buffer[bufsize];
	int32_t head;
	int32_t tail;
};





void ioqueue_init(struct ioqueue* ioq);

bool ioq_empty(struct ioqueue* ioq);

bool ioq_full(struct ioqueue* ioq);

void ioq_wait(struct task_struct** waiter);

void ioq_wakeup(struct task_struct** waiter);

char ioq_getchar(struct ioqueue* ioq);

void ioq_putchar(struct ioqueue* ioq, char byte);

uint32_t ioq_length(struct ioqueue* ioq);
#endif
