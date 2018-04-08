#include "string.h"
#include "global.h"
#include "debug.h"
#include "../stdint.h"

void memset(void* dst_, uint8_t value, uint32_t size)
{
	ASSERT(dst_ != 0);
	uint8_t *dst = (uint8_t*)dst_;
	while (size--) 
		*dst++ = value;
}


void memecpy(void* dst_, void* src_, uint32_t size)
{
	ASSERT(dst_ != 0  && src_ != 0);
	uint8_t* dst = dst_;
	const uint8_t* src = src_;
	while (size-- > 0)
		*dst++ = *src++;
}

int memcmp(const void* a_, const void* b_, uint32_t size)
{
	const char* a = a_;
	const char* b = b_;
	ASSERT(a != 0 && b != 0);
	while (size-- > 0)
	{
		if (*a != *b) return *a > *b ? 1 : -1;
		a++, b++;
	}
	return 0;
}

char* strcpy(char* dst_, char* src_)
{
	char* dst = dst_;
	const char* src = src_;
	ASSERT(src != 0);
	while (src[0])
	    dst[0] = src[0], dst++, src++;
}

uint32_t strlen(const char* str)
{
	ASSERT(str != 0);
	const char* p = str;
	uint32_t count = 0;
	while (p[0]) count++, p++;
	return count;

}

uint8_t strcmp(const char* a, const char* b)
{
	ASSERT(a != 0 && b != 0);
	while (*a != 0 && *b == *a)
	{
		a++, b++;
	}
	return *a < *b ? -1 : *a > *b;
}

char* strchr(const char* str, const uint8_t ch)
{
	ASSERT(str != 0);
	while (*str)
	{
		if (*str == ch)
			return (char*)str;
		str++;
	}
	return 0;
}

char* strrchr(const char* str, const char ch)
{
	ASSERT(str != 0);
	const char* last_char = 0;
	while (*str)
	{
		if (*str == ch) last_char = str;
		str++;
	}
	return (char*)last_char;
}

char* strcat(char* dst_, const char* src_)
{
	ASSERT(dst_ != 0 && src_ != 0);
	char* dst = dst_;
	char* src = (char*)src_;
	while (*dst) dst++;
	dst--;
	while (*src)
	{
		*dst = *src;
		dst++, src++;
	}
return dst_;
}

uint32_t strchrs(const char* str, char ch)
{
	ASSERT(str != 0);
	uint32_t ch_cnt = 0;
	const char* p = str;
	while (*p)
	{
		if (*p == ch) ch_cnt++;
		p++;
	}
	return ch_cnt;
}


