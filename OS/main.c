#include "./lib/kernel/print.h"
void main()
{
	put_str("I am kernel\n");
	put_char('A');
	put_str(" : ");
	put_int(9);
	put_char('\n');
	put_str("number: ");
	put_int(0x0000001);
	while(1);
}
