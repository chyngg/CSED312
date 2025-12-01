#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

struct thread;
struct vm_entry;

struct frame {
    void *page_addr;
    struct vm_entry *vm_entry_;
    struct thread *thread;
    bool pinned;

    struct list_elem ft_elem;
};

void frame_table_init(void);

void *frame_alloc(enum palloc_flags flags, struct vm_entry *vme);
void frame_free(void *page_addr);
void frame_free_thread(struct thread *t);
struct frame *frame_evict(void);

void frame_pin(void *page_addr);
void frame_unpin(void *page_addr);

#endif /* vm/frame.h */
