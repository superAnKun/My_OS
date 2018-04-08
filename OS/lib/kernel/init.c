#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "memory.h"
#include "../device/timer.h"
#include "../thread/thread.h"
#include "../device/keyboard.h"
void init_all()
{
	put_str("init_all\n");
	idt_init();
	timer_init();
	mem_init();
	thread_init();
	console_init();
	keyboard_init();
}