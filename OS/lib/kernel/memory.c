#include "memory.h"
#include "print.h"
#include "debug.h"
#include "string.h"
#include "../thread/sync.h"
#define PG_SIZE 4096  //页大小 4 × 1024 1k = 1024字节

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

#define MEM_BITMAP_BASE 0xc009a000

#define K_HEAP_START 0xc0100000

struct pool {
	struct bitmap pool_bitmap;
	uint32_t phy_addr_start;
	uint32_t pool_size;
	struct lock lock;
};

struct arena {
	struct mem_block_desc* desc;
	uint32_t cnt;
	bool large;
};

struct mem_block_desc k_block_descs[DESC_CNT];

struct pool kernel_pool, user_pool;
struct virtual_addr kernel_vaddr;

static void mem_pool_init(uint32_t all_mem) {
	put_str(" mem_pool_init start\n");
	uint32_t page_table_size = PG_SIZE * 256;     //第一个页表填充了256个页表项 其余的未填充
	uint32_t used_mem = page_table_size + 0x100000;  //已用内存 ==  低端1M加256个页表项
	uint32_t free_mem = all_mem - used_mem;
	uint16_t all_free_pages = free_mem / PG_SIZE;
	uint16_t kernel_free_pages = all_free_pages / 2;
	uint16_t user_free_pages = all_free_pages - kernel_free_pages;
	
	//一页为一位的映射 一个char可以表示八页
	uint32_t kbm_length = kernel_free_pages / 8;
	uint32_t ubm_length = user_free_pages / 8;

	//内核内存池的起始地址 内核大小加页目录表和页表的大小
	uint32_t kp_start = used_mem;
	uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;
	kernel_pool.phy_addr_start = kp_start;
	user_pool.phy_addr_start = up_start;

	kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
	user_pool.pool_size = user_free_pages * PG_SIZE;

	kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
	user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

	kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
	user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

	put_str("   kernel_pool_bitmap_start:");
	put_int((int)kernel_pool.pool_bitmap.bits);
	put_str("   kernel_pool_phy_addr_start:");
	put_int(kernel_pool.phy_addr_start);
	put_str("\n");
	put_str("user_pool_bitmap_start:");
	put_int((int)user_pool.pool_bitmap.bits);
	put_str("user_pool_phy_addr_start:");
	put_int(user_pool.phy_addr_start);
	put_str("\n");



	bitmap_init(&kernel_pool.pool_bitmap);
	bitmap_init(&user_pool.pool_bitmap);

	kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
	//位图的数组指向一块未使用的内存，目前定位在内核内存池和用户内存池之外
	kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

	//虚拟内核的映射表 起始地址位堆的起始地址
	kernel_vaddr.vaddr_start = K_HEAP_START;
	bitmap_init(&kernel_vaddr.vaddr_bitmap);
	put_str("   mem_pool_init done\n");
}

/*
void mem_init() {
	put_str("mem_init start\n");
	uint32_t mem_bytes_total = *((uint32_t*)(0xb03));
	put_int(mem_bytes_total);
	mem_pool_init(mem_bytes_total);
	lock_init(&kernel_pool.lock, 1);
	lock_init(&user_pool.lock, 1);
	put_str("mem_init done\n");
}
*/

static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) return 0;
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        //用户内存池 待定
		struct task_struct *cur = running_thread();
		bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
		if (bit_idx_start == -1) return 0;
		while (cnt < pg_cnt) {
			bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
		}
		vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
		ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void*)vaddr_start;
}

uint32_t *pte_ptr(uint32_t vaddr) {
    uint32_t *pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

uint32_t *pde_ptr(uint32_t vaddr) {
    uint32_t *pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

static void* palloc(struct pool *m_pool) {
	//put_str("palloc 1\n");
    uint32_t bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
	//put_str("palloc 2\n");
    if (bit_idx == -1) {
        return 0;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

static void page_table_add(void *_vaddr, void *_page_phyaddr) {
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t *pde = pde_ptr(vaddr);
    uint32_t *pte = pte_ptr(vaddr);

    if (*pde & 0x00000001) {
        ASSERT(!(*pte & 0x00000001));
        if (!(*pte & 0x00000001)) {
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        } else {
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    } else {
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

void *malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == 0) return 0;
    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    while (cnt--) {
        void *page_phyaddr = palloc(mem_pool);
        if (page_phyaddr == 0) return 0;
        page_table_add((void*)vaddr, page_phyaddr);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}


void *get_kernel_pages(uint32_t pg_cnt) {
    void *vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != 0) {
       memset((uint8_t*)vaddr, 0, PG_SIZE * pg_cnt);
    }
    return vaddr;
}

void* get_user_pages(uint32_t pg_cnt) {
	lock_acquire(&user_pool.lock);
	void* vaddr = malloc_page(PF_USER, pg_cnt);
	memset(vaddr, 0, pg_cnt * PG_SIZE);
	lock_release(&user_pool.lock);
	return vaddr;
}

void *get_a_page(enum pool_flags pf, uint32_t vaddr) {
	struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);
	struct task_struct *cur = running_thread();
	int32_t bit_idx = -1;
	if (cur->pgdir != NULL && pf == PF_USER) {
		bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx >= 0);
		bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
	} else if (cur->pgdir == NULL && pf == PF_KERNEL) {
		bit_idx = (vaddr - kernel_vaddr.vaddr_start, bit_idx, 1) / PG_SIZE;
		ASSERT(bit_idx > 0);
		bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
	} else {
		PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
	}
	//console_put_str("ffff1111111\n");
	void *page_phyaddr = palloc(mem_pool);
	//console_put_str("fffff222222222\n");
	if (page_phyaddr == NULL) return NULL;
	page_table_add((void*)vaddr, page_phyaddr);
	lock_release(&mem_pool->lock);
	return (void*)vaddr;
}

uint32_t addr_v2p(uint32_t vaddr) {
	uint32_t *pte = pte_ptr(vaddr);
	return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

void block_desc_init(struct mem_block_desc* desc_array) {
	uint16_t desc_idx, block_size = 16;
	for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
		desc_array[desc_idx].block_size = block_size;
		desc_array[desc_idx].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;
		list_init(&desc_array[desc_idx].free_list);
		block_size <<= 1;
	}
}

void mem_init() {
	put_str("mem_init start\n");
	uint32_t mem_bytes_total = *((uint32_t*)(0xb03));
	put_int(mem_bytes_total);
	mem_pool_init(mem_bytes_total);
	block_desc_init(&k_block_descs);
	lock_init(&kernel_pool.lock, 1);
	lock_init(&user_pool.lock, 1);
	put_str("mem_init done\n");
}


static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
	return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

static struct arena* block2arena(struct mem_block* b) {
	return (struct arena*)((uint32_t)b & 0xfffff000);
}

void* sys_malloc(uint32_t size) {
	enum pool_flags PF;
	struct pool* mem_pool;
	uint32_t pool_size;
	struct mem_block_desc* descs;
	struct task_struct* cur_thread = running_thread();
	if (cur_thread->pgdir == NULL) {
		PF = PF_KERNEL;
		pool_size = kernel_pool.pool_size;
		mem_pool = &kernel_pool;
		descs = k_block_descs;
	} else {
		PF = PF_USER;
		pool_size = user_pool.pool_size;
		mem_pool = &user_pool;
		descs = cur_thread->u_block_desc;
	}
	if (size <= 0 || size >= pool_size) return NULL;
	struct arena* a;
	struct mem_block* b;
	lock_acquire(&mem_pool->lock);
	if (size > 1024) {
		uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);
		a = malloc_page(PF, page_cnt);
		if (a == NULL) {
			lock_release(&mem_pool->lock);
		}
		memset(a, 0, page_cnt * PG_SIZE);
		a->desc = NULL;
		a->cnt = page_cnt;
		a->large = true;
		lock_release(&mem_pool->lock);
		return (void*)(a + 1);
	} else {
		uint8_t desc_idx = 0;
		for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
			if (size <= descs[desc_idx].block_size) break;
		}

		//printf("error111 %d  size: %d\n", desc_idx, descs[desc_idx].block_size);
		if (list_empty(&(descs[desc_idx].free_list))) {
		//	printf("error2222\n");
			a = malloc_page(PF, 1);
		//	printf("error3333\n");
			if (a == NULL) {
				lock_release(&mem_pool->lock);
				return NULL;
			}
			memset(a, 0, PG_SIZE);
			a->desc = &descs[desc_idx];
			a->large = false;
			a->cnt = descs[desc_idx].blocks_per_arena;
			uint32_t block_idx;
			enum intr_status old_status = intr_disable();
			//printf("index: %d\n", descs[desc_idx].blocks_per_arena);
			for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++) {
				b = arena2block(a, block_idx);
				ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
				list_append(&a->desc->free_list, &b->free_elem);
			}
		//	printf("error555\n");
			intr_set_status(old_status);
		}
		b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
		memset(b, 0, descs[desc_idx].block_size);
		a = block2arena(b);
		a->cnt--;
		lock_release(&mem_pool->lock);
		return (void*)b;
	}
}

void pfree(uint32_t pg_phy_addr) {
	struct pool* mem_pool;
	uint32_t bit_idx = 0;
	if (pg_phy_addr >= user_pool.phy_addr_start) {
		mem_pool = &user_pool;
		bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
	} else {
		mem_pool = &kernel_pool;
		bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
	}
	bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

static void page_table_pte_remove(uint32_t vaddr) {
	uint32_t* pte = pte_ptr(vaddr);
	*pte &= ~PG_P_1;
	asm volatile ("invlpg %0" :: "m" (vaddr): "memory");  //更新块表 TLB
}

static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
	uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;
	if (pf == PF_KERNEL) {
		bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
		while (cnt < pg_cnt) {
			bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
		}
	} else {
		struct task_struct* cur_thread = running_thread();
		bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
		while (cnt < pg_cnt) {
			bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
		}
	}
}

void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
	uint32_t pg_phy_addr;
	uint32_t vaddr = (uint32_t)_vaddr, page_cnt = 0;
	ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
	pg_phy_addr = addr_v2p(vaddr);
	if (pg_phy_addr >= user_pool.phy_addr_start) {
		vaddr -= PG_SIZE;
		while (page_cnt < pg_cnt) {
			vaddr += PG_SIZE;
			pg_phy_addr = addr_v2p(vaddr);
			ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);
			pfree(pg_phy_addr);
			page_table_pte_remove(vaddr);
			page_cnt++;
		}
		vaddr_remove(pf, _vaddr, pg_cnt);
	} else {
		vaddr -= PG_SIZE;
		while (page_cnt < pg_cnt) {
			vaddr += PG_SIZE;
			pg_phy_addr = addr_v2p(vaddr);
			ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= kernel_pool.phy_addr_start && pg_phy_addr < user_pool.phy_addr_start);
			pfree(pg_phy_addr);
			page_table_pte_remove(vaddr);
			page_cnt++;
		}
		vaddr_remove(pf, _vaddr, pg_cnt);
	}
}

void sys_free(void *ptr) {
	ASSERT(ptr != NULL);
	if (ptr == NULL) return;
	enum pool_flags PF;
	struct pool* mem_pool;
	if (running_thread()->pgdir == NULL) {
		PF = PF_KERNEL;
		mem_pool = &kernel_pool;
	} else {
		PF = PF_USER;
		mem_pool = &user_pool;
	}
	lock_acquire(&mem_pool->lock);
	struct mem_block* b = ptr;
	struct arena* a = block2arena(b);
	ASSERT(a->large == 0 || a->large == 1);
	if (a->desc == NULL && a->large == true) {
		mfree_page(PF, a, a->cnt);
	} else {
		list_append(&a->desc->free_list, &b->free_elem);
		if (++a->cnt == a->desc->blocks_per_arena) {
			uint32_t block_idx = 0;
			for (block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++) {
				struct mem_block* b = arena2block(a, block_idx);
				ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
				list_remove(&b->free_elem);
			}
			mfree_page(PF, a, 1);
		}
	}
	lock_release(&mem_pool->lock);
}


//和get_a_page函数相似 但是没有设置位图
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr) {
	struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);
	void* page_phyaddr = palloc(mem_pool);
	if (page_phyaddr == NULL) {
		lock_release(&mem_pool->lock);
		return NULL;
	}
	page_table_add((void*)vaddr, page_phyaddr);
	lock_release(&mem_pool->lock);
	return (void*)vaddr;
}

void free_a_page(uint32_t pg_phy_addr) {
	struct pool* mem_pool;
	uint32_t bit_idx = 0;
	if (pg_phy_addr >= user_pool.phy_addr_start) {
		mem_pool = &user_pool;
		bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
	} else {
		mem_pool = &kernel_pool;
		bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
	}
	bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

