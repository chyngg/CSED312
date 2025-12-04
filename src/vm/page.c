#include "vm/page.h"

#include <debug.h>
#include <string.h>
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "lib/kernel/hash.h"

#define MAX_STACK_SIZE (1 << 23)

extern struct lock file_system_lock;

/* Hash helpers */
// vme의 vaddr을 기준으로 해시 값 계산
static unsigned vm_hash_func (const struct hash_elem *e, void *aux) {
    struct vm_entry *vme = hash_entry (e, struct vm_entry, elem);
    return hash_bytes (&vme->vaddr, sizeof (vme->vaddr));
}  

// vaddr 기준 정렬 시 사용
static bool vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    struct vm_entry *vme_a = hash_entry (a, struct vm_entry, elem);
    struct vm_entry *vme_b = hash_entry (b, struct vm_entry, elem);
    return vme_a->vaddr < vme_b->vaddr;
}

// hash_destroy에서 호출할 entry 삭제 함수. vm_entry 구조체만 free
static void vm_destroy_action (struct hash_elem *e, void *aux) {
    struct vm_entry *vme = hash_entry (e, struct vm_entry, elem);
    if (vme->swap_slot != (size_t)-1) {
        swap_free (vme->swap_slot);
    }
    free (vme);
}

/* S-page table functions */
void spt_init (struct s_page_table *spt) {
    ASSERT (spt != NULL);
    hash_init (&spt->page_map, vm_hash_func, vm_less_func, NULL);
}

void spt_destroy (struct s_page_table *spt) {
    hash_destroy (&spt->page_map, vm_destroy_action);
}

bool spt_insert (struct s_page_table *spt, struct vm_entry *vme) {
    ASSERT (spt != NULL);
    ASSERT (vme != NULL);
    ASSERT (pg_ofs (vme->vaddr) == 0); // page-aligned여야 함 
    struct hash_elem *result = hash_insert (&spt->page_map, &vme->elem);
    return result == NULL; // NULL이면 삽입 성공
}

struct vm_entry *spt_find (struct s_page_table *spt, void *vaddr) {
    ASSERT (spt != NULL);
    void *upage = pg_round_down (vaddr);

    struct vm_entry vme_tmp;
    struct hash_elem *e;

    vme_tmp.vaddr = upage;
    e = hash_find(&spt->page_map, &vme_tmp.elem);
    if (e == NULL)
        return NULL;

    return hash_entry (e, struct vm_entry, elem);
}

void spt_remove (struct s_page_table *spt, struct vm_entry *vme) {
    ASSERT (spt != NULL);
    ASSERT (vme != NULL);

    hash_delete (&spt->page_map, &vme->elem);
    free (vme);
}

bool load_file (void *kaddr, struct vm_entry *vme) {
    ASSERT (vme != NULL);
    ASSERT (kaddr != NULL);
    ASSERT (vme->file != NULL);
    ASSERT (vme->read_bytes + vme->zero_bytes <= PGSIZE);

    int bytes_read;

    lock_acquire(&file_system_lock);
    bytes_read = file_read_at (vme->file, kaddr, vme->read_bytes, vme->offset);
    lock_release(&file_system_lock);

    if (bytes_read != (int)vme->read_bytes) {
        return false;
    }

    memset ((uint8_t *)kaddr + vme->read_bytes, 0, vme->zero_bytes);
    return true;
}

bool handle_fault (struct vm_entry *vme) {
    ASSERT (vme != NULL);
    ASSERT (!vme->is_loaded);

    struct thread *t = thread_current();

    // if page is swapped out, reload
    if (vme->swap_slot != (size_t)-1) {
        void *kaddr = frame_alloc (PAL_USER, vme);
        if (kaddr == NULL)
            return false;

        if (!swap_in (vme, kaddr)) {
            frame_free (kaddr);
            return false;
        }

        if (!pagedir_set_page (t->pagedir, vme->vaddr, kaddr, vme->writable)) {
            frame_free (kaddr);
            return false;
        }

        vme->is_loaded = true;
        frame_unpin (kaddr);
        return true;
    }

    void *kaddr = frame_alloc (PAL_USER, vme);
    if (kaddr == NULL) 
        return false;

    bool success = false;

    switch (vme->type) {
        case VM_BIN:
        case VM_FILE:
            success = load_file (kaddr, vme);
            break;
        case VM_ANON:
            success = swap_in (vme, kaddr);
            break;
        default:
            success = false;
            break;
    }

    if (!success) {
        frame_free (kaddr);
        return false;
    }

    if (!pagedir_set_page (t->pagedir, vme->vaddr, kaddr, vme->writable)) {
        frame_free (kaddr);
        return false;
    }

    vme->is_loaded = true;
    frame_unpin (kaddr);
    return true;
}

bool check_stack_boundary (void *fault_addr, void *esp) {
    if (!is_user_vaddr (fault_addr))
        return false;
    
    void *stack_limit = (uint8_t *)PHYS_BASE - MAX_STACK_SIZE;
    if (fault_addr < stack_limit)
        return false;

    if (fault_addr < (uint8_t *)esp - 32)
        return false;

    return true;
}

bool expand_stack (void *upage) {
    struct thread *t = thread_current();
    void *page = pg_round_down (upage);

    if (spt_find (&t->spt, page) != NULL)
        return true;

    struct vm_entry *vme = malloc (sizeof *vme);
    if (vme == NULL)
        return false;

    vme->type = VM_ANON;
    vme->vaddr = page;
    vme->writable = true;
    vme->file = NULL;
    vme->offset = 0;
    vme->read_bytes = 0;
    vme->zero_bytes = PGSIZE;
    vme->swap_slot = (size_t)-1;
    vme->is_loaded = false;

    void *kaddr = frame_alloc (PAL_USER, vme);
    if (kaddr == NULL) {
        free(vme);
        return false;
    }

    memset (kaddr, 0, PGSIZE);

    if (!pagedir_set_page (t->pagedir, vme->vaddr, kaddr, vme->writable)) {
        frame_free (kaddr);
        free (vme);
        return false;
    }

    vme->is_loaded = true;

    if (!spt_insert (&t->spt, vme)) {
        pagedir_clear_page (t->pagedir, page);
        frame_free (kaddr);
        free (vme);
        return false;
    }
    frame_unpin (kaddr);

    return true;
}