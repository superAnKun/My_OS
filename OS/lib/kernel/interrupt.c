#include "interrupt.h"
#include "../stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define IDT_DESC_CNT 0x30     //总共的中断数量
#define PIC_M_CTRL 0x20        //8259A主片的控制端口是0x20
#define PIC_M_DATA 0x21        //8259A主片的数据端口是0x21
#define PIC_S_CTRL 0xa0        //8259A从片的控制端口
#define PIC_S_DATA 0xa1        //8259A从片的数据端口


#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAGS_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAGS_VAR))





struct gate_desc
{
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount;

    uint8_t attribute;
    uint16_t func_offset_high_word;
};

static struct gate_desc idt[IDT_DESC_CNT];   //中断描述符表数组
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);   //填充中断描述符表的函数


extern intr_handler intr_entry_table[IDT_DESC_CNT];


char* intr_name[IDT_DESC_CNT];
intr_handler idt_table[IDT_DESC_CNT];



static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function)
{
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000ffff;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xffff0000) >> 16;
}

//初始化中断描述符表
static void idt_desc_init(void)
{
    int i;
    for (i = 0; i < IDT_DESC_CNT; i++)
    {
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }

    put_str("  idt_desc_init done\n");
}

//获取当前中断状态
enum intr_status intr_get_status()
{
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}


//通用中断处理函数
static void general_intr_handler(uint8_t vec_nr)
{
    if(vec_nr == 0x27 || vec_nr == 0x2f)
    {
        return ;
    }

	if (vec_nr == 0x21) {
		put_str("keyboard interrrupt occur!!\n");
		return ;
	}
	
	setCursor(0);
	int cursor_pos = 0;
	while (cursor_pos < 320) {
		put_char(' ');
		cursor_pos++;
	}
	setCursor(0);
	
	put_str("!!!!!!!!!!!!!!! exception message begin !!!!!!!!!!!!!!!!!!!!!!!\n");
	setCursor(88);
	put_str(intr_name[vec_nr]);
    if (vec_nr == 14) {                //如果是缺页异常 就打印出来 错误页在cr2寄存器中存放
		int page_fault_vaddr = 0;
		asm("movl %%cr2, %0" : "=r" (page_fault_vaddr));
		put_str("\npage fault addr is "); 
		put_int(page_fault_vaddr);
	}
	put_str("\n!!!!!!!!  exception message end !!!!!!!!!!!!\n");
    while(1);
}


//中断处理历程注册中断处理函数
static void exception_init(void)
{
    int i;
    for(i = 0; i < IDT_DESC_CNT; i++)
    {
        idt_table[i] = general_intr_handler;
        intr_name[i] = "unknow";
    }

    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Rangle Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    //intr_name[15] = "#"  保留项 不使用
    intr_name[16] = "#MF 0x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";

}


static void pic_init(void)
{
    outb(PIC_M_CTRL, 0x11);
    outb(PIC_M_DATA, 0x20);
    
    outb(PIC_M_DATA, 0x04);
    outb(PIC_M_DATA, 0x01);

    outb(PIC_S_CTRL, 0x11);
    outb(PIC_S_DATA, 0x28);

    outb(PIC_S_DATA, 0x02);
    outb(PIC_S_DATA, 0x01);

 //   outb(PIC_M_DATA, 0xfe);
 //   outb(PIC_S_DATA, 0xff);

	outb(PIC_M_DATA, 0);
	outb(PIC_S_DATA, 0);

    put_str(" pic_init done \n");
}



//开中断 并 返回中断前的状态
enum intr_status intr_enable()
{
    enum intr_status old_status;
    if (INTR_ON == intr_get_status())
    {
        old_status = INTR_ON;
        return old_status;
    }
    asm volatile ("sti");
    old_status = INTR_OFF;
    return old_status;
}


//关中断 并 返回中断前的状态
enum intr_status intr_disable()
{
    enum intr_status old_status;
    if (INTR_OFF == intr_get_status())
    {
        old_status = INTR_OFF;
        return old_status;
    }
    asm volatile ("cli" : : : "memory");
    old_status = INTR_ON;
    return old_status;
}

//设置中断 
enum intr_status intr_set_status(enum intr_status status)
{
    return status & INTR_ON ? intr_enable() : intr_disable();
}


//注册相应的中断处理函数
void register_handler(uint8_t vector_no, intr_handler function) {
	idt_table[vector_no] = function;
}

//总初始化
void idt_init()
{   
    put_str("idt_init start\n");
    idt_desc_init();
    pic_init();      //初始化8259A 
    exception_init();

    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)((uint32_t)idt) << 16));
    asm volatile("lidt %0" : : "m"(idt_operand));
    put_str("idt_init done\n");   
}


