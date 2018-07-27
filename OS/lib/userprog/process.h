#ifndef _PROCESS_H
#define _PROCESS_H
#include "../stdint.h"
#include "../thread/thread.h"
#include "../thread/sync.h"
#include "../kernel/global.h"
#include "../kernel/string.h"
#define USER_VADDR_START 0x8048000    //用户虚拟地址的起始地址

void process_execute(void* filename, char* name);
int32_t sys_execv(const char* path, const char* argv[]);
#endif
