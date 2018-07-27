#ifndef __STDIO_H_
#define __STDIO_H_
#include "../user/syscall.h"
#include "../stdint.h"
int32_t printf(const char* format, ...);
uint32_t sprintf(char *buf, const char* format, ...);

#define va_start(ap, v) ap = (va_list)&v;
#define va_arg(ap, t) *((t*)(ap += 4))
#define va_end(ap) ap = NULL
#endif
