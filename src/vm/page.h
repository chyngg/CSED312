#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "filesys/file.h"
#include "lib/kernel/hash.h"

// page 종류
enum vm_type {
    VM_BIN, // 실행 파일, data segment
    VM_ANON, // stack, heap
    VM_FILE // mmap 등 파일 매핑
};

struct vm_entry {
	uint8_t type; // VM_BIN, VM_FILE, VM_ANON 등
	void *vaddr; // page 기준 user virtual address
	bool writable; // 쓰기 가능 여부
	
	struct file *file; // backing file
	off_t offset; // 파일 내에서의 시작 위치
	size_t read_bytes; // 파일에서 읽어야 하는 바이트 수
	size_t zero_bytes; // 나머지 0으로 채울 바이트 수
	
	size_t swap_slot; // swap 영역 위치
	
	bool is_loaded; // 현재 physical memory에 올라와 있는지 여부
	
	struct hash_elem elem; // S-page table(hash)의 element
};

struct s_page_table {
    struct hash page_map; // vm_entry를 저장하는 해시 테이블
};

bool handle_fault (struct vm_entry *vme);
bool load_file (void *kaddr, struct vm_entry *vme);

void spt_init (struct s_page_table *spt);
void spt_destroy (struct s_page_table *spt);
bool spt_insert (struct s_page_table *spt, struct vm_entry *vme);
struct vm_entry *spt_find (struct s_page_table *spt, void *vaddr);
void spt_remove (struct s_page_table *spt, struct vm_entry *vme);

bool check_stack_boundary (void *fault_addr, void *esp);
bool expand_stack (void *upage);

#endif /* vm/page.h */