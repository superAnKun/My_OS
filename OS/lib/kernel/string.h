#ifndef _STRING_H_
#define _STRING_H_
#include "../stdint.h"
#include "global.h"
#include "print.h"
#include "debug.h"
void memset(void*, uint8_t, uint32_t);
void memcpy(void*, void*, uint32_t);
int memcmp(const void*, const void*, uint32_t);
char *strcpy(char*, char*);
uint32_t strlen(const char*);
uint8_t strcmp(const char*, const char*);
char *strchr(const char*, const uint8_t);
char* strrchr(const char*, const char);
char* strcat(char*, const char*);
uint32_t strchrs(const char*, char);

#endif
