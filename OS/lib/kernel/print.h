#ifndef  _LIB_KERNEL_PRINT_H
#define  _LIB_KERNEL_PRINT_H
#include "../stdint.h"
void put_char(uint8_t char_asci);
void put_str(char* message);
void put_int(unsigned int num);
void setCursor(uint16_t num);
void cls_screen();
void printk(const char* format, ...);
#endif
