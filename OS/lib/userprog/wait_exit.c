#include "../thread/thread.h"

static void release_prog_resource(struct task_struct* release_thread) {
	
	uint32_t* pgdir_vaddr = release_thread->pgdir;
	uint16_t user_pde_nr = 768, pde_idx = 0;
	uint32_t pde = 0;
	uint32_t* v_pde_ptr = NULL;

	struct virtual_addr* prog_addr = &release_thread->userprog_vaddr;
	uint32_t bit_idx = 0;
	while (bit_idx < prog_addr->vaddr_bitmap.btmp_bytes_len) {
		uint8_t grain = prog_addr->vaddr_bitmap.bits[bit_idx];
		for (int i = 0; i < 8; i++) {
			if (!(grain & (1 << i))) continue;
			uint32_t vaddr = prog_addr->vaddr_start + (bit_idx * 8 + i) * 4096;
			uint32_t pte = pte_ptr(vaddr);
			ASSERT(pte & 0x00000001);
			if (pte & 0x00000001) {
				uint32_t pg_phy_addr = pte & 0xfffff000;
				free_a_page(pg_phy_addr);
			}
		}
		bit_idx++;
	}

	while (pde_idx < user_pde_nr) {
		v_pde_ptr = pgdir_vaddr + pde_idx;
		pde = *v_pde_ptr;
		uint32_t pg_phy_addr = pde & 0xfffff000;
		free_a_page(pg_phy_addr);
		pde_idx++;
	}

	//回收虚拟内存池
	uint32_t bitmap_pg_cnt = (release_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len) / PG_SIZE;
	uint8_t* user_vaddr_pool_bitmap = release_thread->userprog_vaddr.vaddr_bitmap.bits;
	mfree_page(PF_KERNEL, user_vaddr_pool_bitmap, bitmap_pg_cnt); //从内核中申请的内存在内核中释放掉
	uint8_t fd_idx = 3;
	while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
		if (release_thread->fd_table[fd_idx] != -1) sys_close(fd_idx);
		fd_idx++;
	}
}

static bool find_child(struct list_elem* elem, int32_t ppid) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, elem);
	if (pthread->parent_pid == ppid) return true;
	return false;
}

static bool find_hanging_child(struct list_elem* pelem, int32_t ppid) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	if (pthread->parent_pid == ppid && pthread->status == TASK_HANGING) return true;
	return false;
}

static bool init_adopt_a_child(struct list_elem* pelem, int32_t pid) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	if (pthread->parent_pid == pid) pthread->parent_pid = 1;
	return false;
}

pid_t sys_wait(int32_t* status) {
	struct task_struct* parent_thread = running_thread();
	while (1) {
		struct list_elem* child_elem = list_traversal(&thread_all_list, find_hanging_child, parent_thread->pid);
		if (child_elem != NULL) {
			struct task_struct* child_thread = elem2entry(struct task_struct, all_list_tag, child_elem);
			uint16_t child_pid = child_thread->pid;
			int8_t aa = child_thread->exit_status;
			*status = (int32_t)child_thread->exit_status;
			thread_exit(child_thread, false);
			return child_pid;
		}
		child_elem = list_traversal(&thread_all_list, find_child, parent_thread->pid);
		if (child_elem == NULL) return -1;
		thread_block(TASK_WAITING);
	}
}

void sys_exit(int32_t status) {
	struct task_struct* child_thread = running_thread();
	child_thread->exit_status = status;
	if (child_thread->parent_pid == -1) PANIC("sys_exit: child_thread->parent_pid is -1\n");
	list_traversal(&thread_all_list, init_adopt_a_child, child_thread->pid);
	release_prog_resource(child_thread);
	struct task_struct* parent_thread = pid2thread(child_thread->parent_pid);
	if (parent_thread->status == TASK_WAITING) {
		thread_unblock(parent_thread);
	}
	thread_block(TASK_HANGING);
}
