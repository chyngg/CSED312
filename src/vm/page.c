#include "vm/page.h"

#include <debug.h>
#include <string.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"

extern struct lock file_system_lock;

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