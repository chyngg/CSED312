#include "vm/swap.h"

#include <debug.h>
#include <stdint.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

static struct block *swap_block;
static struct bitmap *swap_table;
static struct lock swap_lock;

extern struct lock file_system_lock;

void
swap_init (void)
{
  swap_block = block_get_role (BLOCK_SWAP);
  ASSERT (swap_block != NULL);

  size_t slot_cnt = block_size (swap_block) / SECTORS_PER_PAGE;
  swap_table = bitmap_create (slot_cnt);
  ASSERT (swap_table != NULL);
  bitmap_set_all (swap_table, false);

  lock_init (&swap_lock);
}

bool
swap_out (struct frame *f)
{
  ASSERT (f != NULL);
  ASSERT (f->vm_entry_ != NULL);
  ASSERT (f->thread != NULL);

  struct vm_entry *vme = f->vm_entry_;
  struct thread *t = f->thread;
  void *kpage = f->page_addr;

  if (vme->type == VM_FILE)
    {
      if (pagedir_is_dirty (t->pagedir, vme->vaddr))
        {
          lock_acquire (&file_system_lock);
          file_write_at (vme->file, kpage, vme->read_bytes, vme->offset);
          lock_release (&file_system_lock);
        }

      pagedir_clear_page (t->pagedir, vme->vaddr);
      vme->is_loaded = false;
      return true;
    }

  lock_acquire (&swap_lock);
  size_t slot = bitmap_scan_and_flip (swap_table, 0, 1, false);
  if (slot == BITMAP_ERROR)
    {
      lock_release (&swap_lock);
      return false;
    }

  size_t sector_base = slot * SECTORS_PER_PAGE;
  for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
    {
      block_write (swap_block, sector_base + i,
                   (uint8_t *) kpage + i * BLOCK_SECTOR_SIZE);
    }
  lock_release (&swap_lock);

  vme->swap_slot = slot;
  if (vme->type == VM_BIN){
    vme->type = VM_ANON; // anonymous
  }
  vme->is_loaded = false;
  pagedir_clear_page (t->pagedir, vme->vaddr);
  return true;
}

bool
swap_in (struct vm_entry *vme, void *kaddr)
{
  ASSERT (vme != NULL);
  ASSERT (kaddr != NULL);
  ASSERT (swap_block != NULL);
  ASSERT (vme->swap_slot != (size_t) -1);

  size_t slot = vme->swap_slot;
  size_t sector_base = slot * SECTORS_PER_PAGE;

  lock_acquire (&swap_lock);
  if (slot >= bitmap_size (swap_table))
    {
      lock_release (&swap_lock);
      return false;
    }

  for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
    block_read (swap_block, sector_base + i,
                (uint8_t *) kaddr + i * BLOCK_SECTOR_SIZE);

  bitmap_set (swap_table, slot, false);
  lock_release (&swap_lock);

  vme->swap_slot = (size_t) -1;
  return true;
}

void
swap_free (size_t slot)
{
  lock_acquire (&swap_lock);
  if (slot < bitmap_size (swap_table))
    bitmap_set (swap_table, slot, false);
  lock_release (&swap_lock);
}
