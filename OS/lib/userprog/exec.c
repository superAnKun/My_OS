#include "../thread/thread.h"
#include "process.h"
#include "../fs/file.h"
#include "../fs/inode.h"
extern void intr_exit();
#define TASK_NAME_LEN 16
typedef uint32_t Elf32_t, Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

struct Elf32_Ehdr {
  unsigned char	e_ident[16];	/* Magic number and other info */
  Elf32_Half	e_type;			/* Object file type */
  Elf32_Half	e_machine;		/* Architecture */
  Elf32_Word	e_version;		/* Object file version */
  Elf32_Addr	e_entry;		/* Entry point virtual address */
  Elf32_Off	e_phoff;		/* Program header table file offset */
  Elf32_Off	e_shoff;		/* Section header table file offset */
  Elf32_Word	e_flags;		/* Processor-specific flags */
  Elf32_Half	e_ehsize;		/* ELF header size in bytes */
  Elf32_Half	e_phentsize;		/* Program header table entry size */
  Elf32_Half	e_phnum;		/* Program header table entry count */
  Elf32_Half	e_shentsize;		/* Section header table entry size */
  Elf32_Half	e_shnum;		/* Section header table entry count */
  Elf32_Half	e_shstrndx;		/* Section header string table index */
};

struct Elf32_Phdr {
  Elf32_Word	p_type;			/* Segment type */
  Elf32_Off	p_offset;		/* Segment file offset */
  Elf32_Addr	p_vaddr;		/* Segment virtual address */
  Elf32_Addr	p_paddr;		/* Segment physical address */
  Elf32_Word	p_filesz;		/* Segment size in file */
  Elf32_Word	p_memsz;		/* Segment size in memory */
  Elf32_Word	p_flags;		/* Segment flags */
  Elf32_Word	p_align;		/* Segment alignment */
};

//段类型
enum segment_type {
	PT_NULL,    //忽略
	PT_LOAD,    //可加载程序段
	PT_DYNAMIC,  //动态加载信息
	PT_INTERP,   //动态加载器名称
	PT_NOTE,    //一些辅助信息
	PT_SHLIB,   //保留
	PT_PHDR    //程序头表
};


//从文件系统中 把 fd所代表的文件的某一个段 读入内存vaddr处
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr) {
	//printk("vaddr: 0x%x filesz: 0x%x offset: 0x%x\n", vaddr, filesz, offset);
	uint32_t vaddr_first_page = vaddr & 0xfffff000;
	uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);
	uint32_t occupy_pages = 0;
	if (filesz > size_in_first_page) {
		uint32_t left_size = filesz - size_in_first_page;
		occupy_pages = DIV_ROUND_UP(left_size, PG_SIZE) + 1;
	} else {
		occupy_pages = 1;
	}

	//printk("occupy_pages: %d\n", occupy_pages);
	uint32_t page_idx = 0;
	uint32_t vaddr_page = vaddr_first_page;
	struct task_struct *cur = running_thread();
	while (page_idx < occupy_pages) {
		uint32_t* pde = pde_ptr(vaddr_page);
		uint32_t* pte = pte_ptr(vaddr_page);
		if (!(*pde & 0x00000001) || !(*pte & 0x00000001)) {
	//		printk("vaddr_page: 0x%x start_addr: 0x%x\n", (uint32_t)vaddr_page, (uint32_t)cur->userprog_vaddr.vaddr_start);
			if (get_a_page(PF_USER, vaddr_page) == NULL) {
				return false;
			}
		}
		page_idx++;
		vaddr_page += PG_SIZE;
	}
	sys_lseek(fd, offset, SEEK_SET);
	//printk("vaddr: 0x%x filesz: 0x%x offset:0x%x\n", (uint32_t)vaddr, (uint32_t)filesz, (uint32_t)offset);
	uint32_t times = DIV_ROUND_UP(filesz, 0x1000);
	sys_read(fd, (void*)vaddr, 0x1000 * times + 1);
	return true;
}


//将pathname 载入内存
static int32_t load(const char* pathname) {
	int32_t ret = -1;
	struct Elf32_Ehdr elf_header;
	struct Elf32_Phdr prog_header;
	memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));
	int32_t fd = sys_open(pathname, O_RDONLY);
	if (fd == -1) return -1;
	if (sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) != sizeof(struct Elf32_Ehdr)) {
		ret = -1;
		goto done;
	}
	if (memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) || elf_header.e_type != 2 || elf_header.e_machine != 3 || \
			elf_header.e_version != 1 || elf_header.e_phnum > 1024 || elf_header.e_phentsize != sizeof(struct Elf32_Phdr)) {
		ret = -1;
		goto done;
	}
//printf("e_ident: %s\n", elf_header.e_ident);
	Elf32_Off prog_header_offset = elf_header.e_phoff;
	Elf32_Half prog_head_size = elf_header.e_phentsize;
	uint32_t prog_idx = 0;
	while (prog_idx < elf_header.e_phnum) {
	//	printk("prog_idx: %d segment type: 0x%x\n", prog_idx, prog_header.p_type);
		memset(&prog_header, 0, prog_head_size);
		sys_lseek(fd, prog_header_offset, SEEK_SET);
		//printk("prog_idx: %d segment type: 0x%x\n", prog_idx, prog_header.p_type);
		if (sys_read(fd, &prog_header, prog_head_size) != prog_head_size) {
			ret = -1;
			goto done;
		}
	//	printk("prog_idx: %d segment type: 0x%x vaddr: 0x%x\n", prog_idx, prog_header.p_type, (uint32_t)prog_header.p_vaddr);
		if (PT_LOAD == prog_header.p_type) {
			//printk("PT_LOAD align: 0x%x\n", (uint32_t)prog_header.p_align);
			if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)) {
				ret = -1;
				goto done;
			}
		}
		prog_header_offset += elf_header.e_phentsize;
		prog_idx++;
	}
	ret = elf_header.e_entry;
done:
	sys_close(fd);
	return ret;
}

int32_t sys_execv(const char* path, const char* argv[]) {
	//printk("sys_execv start!!!!!\n");
	uint32_t argc = 0;
	while (argv[argc]) {
		argc++;
	}
	int32_t entry_point = load(path);
	if (entry_point == -1) {
		printk("entry_point is -1\n");
		return -1;
	}
	/*
		uint32_t* pde = pde_ptr(0xc0000000);
		uint32_t* pte = pte_ptr(0xc0000000);
		if (!(*pde & 0x00000001) || !(*pte & 0x00000001)) {
	//		printk("0xc0000000 vaddr_page: 0x%x start_addr: 0x%x\n", (uint32_t)vaddr_page, (uint32_t)cur->userprog_vaddr.vaddr_start);
	        printk("pde pte sdjksjfksjdf\n");
			if (get_a_page(PF_USER, 0xc0000000) == NULL) {
				return false;
			}
		}
*/
	struct task_struct *cur = running_thread();
	memcpy(cur->name, path, TASK_NAME_LEN);
	cur->name[TASK_NAME_LEN - 1] = 0;
	struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));
	intr_0_stack->ebx = (int32_t)argv;
	intr_0_stack->ecx = argc;
	intr_0_stack->eip = (void*)entry_point;
	//intr_0_stack->esp = (void*)0xc0000000;
	//intr_0_stack->esp = (void*)(((uint32_t)intr_0_stack->esp & 0xfffff000) - 1);
	//printk("entry_point: 0x%x intr_0_stack esp: 0x%x\n", (uint32_t)entry_point, (uint32_t)intr_0_stack->esp);

	asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g"(intr_0_stack) : "memory");
	//while (1);
	return 0;
}
