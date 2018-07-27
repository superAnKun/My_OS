#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "../stdint.h"
#include "bitmap.h"
#include "list.h"
#include "../lib/stdio.h"
enum pool_flags {
    PF_KERNEL = 1,  //内核内存池
    PF_USER = 2     //用户内存池
};

#define PG_P_1 1
#define PG_P_0 0
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0
#define PG_US_U 4




struct virtual_addr {
	struct bitmap vaddr_bitmap;
	uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;
void mem_init(void);

struct mem_block {
	struct list_elem free_elem;
};

struct mem_block_desc {
	uint32_t block_size;       //内存块大小
	uint32_t blocks_per_arena; //本arena中可容纳此mem_block的数量
	struct list free_list;    //目前可用的mem_block链表
};
#define DESC_CNT 7
void* sys_malloc(uint32_t);
void sys_free(void*);
void *get_kernel_pages(uint32_t pg_cnt);
#endif
