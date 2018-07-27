#ifndef TSS_H
#define TSS_H
#include "../stdint.h"
#include "../thread/thread.h"
#include "../kernel/print.h"
#include "../kernel/global.h"
void update_tss_esp(struct task_struct *pthread);
void tss_init();
#endif
