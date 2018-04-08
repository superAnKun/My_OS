#include "init.h"
#include "print.h"
#include "debug.h"
void k_thread_a(void*);
void k_thread_b(void*);

void main()
{
	put_str("I am kernel\n");
	init_all();
 //   put_str("get pages\n");
   // void *vaddr = get_kernel_pages(3);
  //  put_str("\n get_kernel_page start vaddr is  ");
  //  put_int((uint32_t)vaddr);
  //  put_str("\n");

//	thread_start("k_thread_a", 31, k_thread_a, "argA ");
//	thread_start("K_thread_b", 8, k_thread_b, "argB ");
	intr_enable();

	//while (1) {
	//	intr_disable();
	//	put_str("main ");
	//	intr_enable();
	//    console_put_str("main ");
	//}
	//asm volatile("sti");
	//ASSERT(1 == 2);
	while(1);
}


void k_thread_a(void *arg) {
	char *str = (char*)arg;
	while (1) {
	//	intr_disable();
	//	put_str(str);
	//	intr_enable();
	    console_put_str(str);
	}
}

void k_thread_b(void *arg) {
	char *str = (char*)arg;
	while (1) {
	//	intr_disable();
	//	put_str(str);
	//	intr_enable();
	    console_put_str(str);
	}
}
