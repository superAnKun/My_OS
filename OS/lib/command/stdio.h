#ifndef __STDIO_H_
#define __STDIO_H_
#include "../stdint.h"
enum std_d {
	stdin_no,
	stdout_no,
	stderr_no
};
#define va_start(ap, v) (ap = (va_list)&v)
#define va_arg(ap, t) (*((t*)(ap += 4)))
#define va_end(ap) (ap = NULL);
int32_t printf(const char* format, ...);
uint32_t sprintf(char *buf, const char* format, ...);
uint32_t vsprintf(char* str, const char* format, va_list ap);
#endif
