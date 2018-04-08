#ifndef _CONSOLE_H_
#define _CONSOLE_H_
#include "../stdint.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
void console_init();
void console_acquire();
void console_release();
void console_put_str(char *);
void console_put_char(uint8_t );
void console_put_int(uint32_t);

#endif
