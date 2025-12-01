#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

struct frame;
struct vm_entry;

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init (void);
bool swap_out (struct frame *f);
bool swap_in (struct vm_entry *vme, void *kaddr);
void swap_free (size_t slot);

#endif /* vm/swap.h */
