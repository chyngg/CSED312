#include "vm/frame.h"
#include <string.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

static struct list frame_table;
static struct lock frame_table_lock;

static struct list_elem *clock_hand;

static struct frame *frame_lookup(void *page_addr);
static struct frame *frame_select_victim(void);

// frame table 초기화
void frame_table_init(void) {
    list_init(&frame_table);
    lock_init(&frame_table_lock);
    clock_hand = NULL;
}   

// 프레임 할당
void *frame_alloc (enum palloc_flags flags, struct vm_entry *vme) {
    ASSERT (flags & PAL_USER); // user pool에서만 사용

    lock_acquire(&frame_table_lock);
    void *kpage = palloc_get_page(flags);
    struct frame *f = NULL;

    if (kpage == NULL) {
        // 프레임이 부족하면 eviction
        f = frame_evict();
        if (f == NULL) {
            lock_release(&frame_table_lock);
            return NULL;
        }
        // victim frame의 page 재사용
        kpage = f->page_addr;

        if (flags & PAL_ZERO) {
            memset(kpage, 0, PGSIZE);
        }
        
    } else {
        // 새로운 프레임을 생성하여 frame table에 추가
        f = (struct frame *)malloc(sizeof *f);
        if (f == NULL) {
            // frame 구조체 할당 실패 시 페이지 반환
            palloc_free_page(kpage);
            lock_release(&frame_table_lock);
            return NULL;
        }
        f->page_addr = kpage;
        f->vm_entry_ = NULL;
        f->thread = NULL;
        f->pinned = false;
        list_push_back(&frame_table, &f->ft_elem);

        // clock hand 초기화
        if (clock_hand == NULL) {
            clock_hand = list_begin(&frame_table);
        }
    }

    // 할당된 프레임 정보 설정
    f->vm_entry_ = vme;
    f->thread = thread_current();
    f->pinned = true;

    lock_release(&frame_table_lock);
    return kpage;
}

// 프레임 해제
void frame_free (void *page_addr) {
    if (page_addr == NULL) {
        return;
    }

    lock_acquire(&frame_table_lock);

    struct frame *f = frame_lookup(page_addr);
    if (f != NULL) {
        // clock hand이 해제되는 프레임을 가리키고 있으면 다음 프레임으로 이동
        if (clock_hand == &f->ft_elem) {
            clock_hand = list_next(clock_hand);
            if (clock_hand == list_end(&frame_table)) {
                clock_hand = list_empty(&frame_table) ? NULL : list_begin(&frame_table);
            }
        }

        list_remove(&f->ft_elem);
        palloc_free_page(page_addr);
        free(f);
    }

    lock_release(&frame_table_lock);

}

void frame_free_thread (struct thread *t) {
    if (t == NULL)
        return;

    lock_acquire(&frame_table_lock);

    struct list_elem *e = list_begin(&frame_table);
    while (e != list_end(&frame_table)) {
        struct frame *f = list_entry(e, struct frame, ft_elem);
        e = list_next(e);

        if (f->thread == t) {
            list_remove (&f->ft_elem);

            if (t->pagedir != NULL && f->vm_entry_ != NULL)
                pagedir_clear_page (t->pagedir, f->vm_entry_->vaddr);

            palloc_free_page (f->page_addr);
            free (f);
        }
    }
    
    lock_release(&frame_table_lock);
}

// Eviction 알고리즘 (clock)으로 victim frame 선택
static struct frame *frame_select_victim (void)
{
    if (list_empty(&frame_table)) {
        return NULL;
    }

    if (clock_hand == NULL || clock_hand == list_end(&frame_table)) {
        clock_hand = list_begin(&frame_table);
    }

    while (1) {
        struct frame *f = list_entry(clock_hand, struct frame, ft_elem);

        // pinned == true인건 건너뜀
        if (!f->pinned && f->vm_entry_ != NULL && f->thread != NULL) {
            struct vm_entry *vme = f->vm_entry_;
            struct thread *t = f->thread;

            // accessed bit로 clock 알고리즘 구현
            bool accessed = pagedir_is_accessed(t->pagedir, vme->vaddr);
            if (accessed) { 
                pagedir_set_accessed(t->pagedir, vme->vaddr, false);
            } else {
                // accessed bit가 0이면 victim으로 선택
                struct frame *victim = f;

                // 다음 eviction을 위해 clock hand 이동
                clock_hand = list_next(clock_hand);
                if (clock_hand == list_end(&frame_table)) 
                    clock_hand = list_begin(&frame_table);  
                
                return victim;
            }
        }

        // 다음 프레임으로 이동
        clock_hand = list_next(clock_hand);
        if (clock_hand == list_end(&frame_table)) {
            clock_hand = list_begin(&frame_table);
        }
    }
}

// 외부에서 사용하는 eviction 함수
struct frame *frame_evict (void) {
    ASSERT (lock_held_by_current_thread(&frame_table_lock));
    ASSERT (!list_empty(&frame_table));

    struct frame *victim = frame_select_victim();
    ASSERT (victim != NULL);
    ASSERT (victim->vm_entry_ != NULL);
    ASSERT (victim->thread != NULL);

    struct vm_entry *vme = victim->vm_entry_;
    struct thread *t = victim->thread;

    // dirty인 경우: file-backed이면 파일에 write back, 아니면 swap으로 내보내기
    bool dirty = pagedir_is_dirty(t->pagedir, vme->vaddr);

    if (vme->type == VM_ANON) {
        swap_out(victim);
    } else {
        if (dirty)
            swap_out(victim);

        else {
            // dirty가 아닌 경우는 단순히 vm_entry의 is_loaded를 false로 설정
            pagedir_clear_page(t->pagedir, vme->vaddr);
            vme->is_loaded = false;
        }
    }


    // victim frame 정보 초기화
    victim->vm_entry_ = NULL;
    victim->thread = NULL;
    victim->pinned = false;

    return victim;
}

void frame_pin (void *page_addr) {
    if (page_addr == NULL) 
        return;
    
    lock_acquire(&frame_table_lock);
    struct frame *f = frame_lookup(page_addr);
    if (f != NULL) {
        f->pinned = true;
    }
    lock_release(&frame_table_lock);
}

void frame_unpin (void *page_addr) {
    if (page_addr == NULL) 
        return;
    
    lock_acquire(&frame_table_lock);
    struct frame *f = frame_lookup(page_addr);
    if (f != NULL) {
        f->pinned = false;
    }
    lock_release(&frame_table_lock);
}

static struct frame *frame_lookup (void *page_addr) {
    struct list_elem *e;

    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, ft_elem);
        if (f->page_addr == page_addr) {
            return f;
        }
    }
    return NULL;
}
