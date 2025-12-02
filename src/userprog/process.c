#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "lib/kernel/hash.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  // 2-2 Argument Passing
  char *fn_copy2;
  char *fn_save;
  char *filename;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  // 2-2 Argument Passing
  fn_copy2 = palloc_get_page (0);
  strlcpy(fn_copy2, file_name, PGSIZE);
  filename = strtok_r(fn_copy2, " ", &fn_save);

  /* 2-2 Argument Passing - process_wait */
  // parent context
  struct wait_status *ws = palloc_get_page(0);
  if (!ws) return TID_ERROR;
  ws->tid = TID_ERROR;
  ws->exit_code = -1;
  ws->waited = false;
  sema_init(&ws->sema, 0);
  sema_init(&ws->load_sema, 0); // load 성공 확인용
  ws->load_success = false; // load 성공 확인용
  list_push_back(&thread_current()->wait_list, &ws->elem);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (filename, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR){
    palloc_free_page (fn_copy); 
    list_remove(&ws->elem); palloc_free_page(ws);
    palloc_free_page(fn_copy2);
    return TID_ERROR;
  }
  // ws->tid = tid;
  struct thread *child = get_child_process(tid);
  if (!child) {
    list_remove(&ws->elem); palloc_free_page(ws);
    palloc_free_page(fn_copy2);
    return TID_ERROR;
  }

  child->wait_status = ws;
  /* 2-2 Argument Passing - process_wait */

  /* 2-2. Argument Passing */
  sema_down(&ws->load_sema);
  if (!ws->load_success) {
    list_remove(&ws->elem);
    palloc_free_page(ws);
    palloc_free_page(fn_copy2);
    return TID_ERROR;
  }

  /*
  sema_down(&child->sema_exec);
  if (!child->loaded) {
    list_remove(&ws->elem); palloc_free_page(ws);
    palloc_free_page(fn_copy2);
    return TID_ERROR;
  }
  */

  /*
  struct list_elem *e;
  struct thread *child_thread;
  for (e = list_begin(&thread_current()->child_list); e != list_end(&thread_current()->child_list); e = list_next(e)) {
    child_thread = list_entry(e, struct thread, child_list_elem);
    if (child_thread->exit_status == -1) 
      return process_wait(tid);
  }
  */
  
  ws->tid = tid;
  palloc_free_page (fn_copy2);
  return tid;
  /* 2-2. Argument Passing */
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  // 2-2 Argument Passing
  char *fn_copy = palloc_get_page (0);
  strlcpy(fn_copy, file_name, PGSIZE);
  int argc;
  char **argv;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  
  /* 2-2 Argument Passing */
  argv = palloc_get_page(0);

  if (argv == NULL) {
    palloc_free_page(file_name);
    palloc_free_page(fn_copy);
    thread_exit();
  }

  argc = fn_to_argv(argv, fn_copy);

  struct wait_status *ws = thread_current()->wait_status;
  
  success = load (argv[0], &if_.eip, &if_.esp);

  thread_current()->loaded = success;

  // load 성공 확인용
  if (ws != NULL) {
    ws->load_success = success;
    sema_up(&ws->load_sema);
  }
  // load 성공 확인용

  if (success) 
    stack_argv(argv, argc, &if_.esp);
  sema_up(&thread_current()->sema_exec);

  palloc_free_page(argv);
  palloc_free_page(fn_copy);
  /* 2-2 Argument Passing */

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  /* 2-2 Argument Passing - process_wait */
  struct thread *cur = thread_current();
  struct wait_status *ws = NULL;

  struct list_elem *e;
  for (e = list_begin(&cur->wait_list); e != list_end(&cur->wait_list); e = list_next(e)) {
    struct wait_status *x = list_entry(e, struct wait_status, elem);
    if (x->tid == child_tid) { ws = x; break; }
  }
  if (!ws) return -1;
  if (ws->waited) return -1;
  ws->waited = true;

  sema_down(&ws->sema);
  int status = ws->exit_code;

  list_remove(&ws->elem);
  palloc_free_page(ws);
  return status;
  /* 2-2 Argument Passing - process_wait */
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* 3 Virtual Memory - On process termination */
  // mmap table
  struct list_elem *mmap_elem = list_begin (&cur->mmap_table);
  while (mmap_elem != list_end (&cur->mmap_table)) {
    struct mmap_entry *m = list_entry (mmap_elem, struct mmap_entry, elem);
    mmap_elem = list_next (mmap_elem);
    sys_munmap (m->id);
  }
  
  frame_free_thread (cur);
  // s-page tables
  spt_destroy (&cur->spt);
  

  /* 3 Virtual Memory - On process termination */

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  /* 2-3. System Call */
  // should clean fd_list 
  if(cur->fd_list){
    for(int fd = 2; fd < cur->fd_next; fd++){
      if(cur->fd_list[fd]){
        lock_acquire(&file_system_lock);
        file_close(cur->fd_list[fd]);
        lock_release(&file_system_lock);
        cur->fd_list[fd] = NULL;
      }
    }
    palloc_free_page(cur->fd_list);
    cur->fd_list = NULL;
  }

  // should remove from child_list
  if (cur->parent != NULL) {
      list_remove(&cur->child_list_elem);
  }

  // should clean wait_list 
  struct list_elem *e;
  struct list_elem *next;
  for (e = list_begin(&cur->wait_list); e != list_end(&cur->wait_list); e = next) {
    next = list_next(e);
    struct wait_status *ws = list_entry(e, struct wait_status, elem);
    list_remove(e);
    palloc_free_page(ws);
  }
  /* 2-3. System Call */

  /* 2-2 Argument Passing - process_wait */
  if (cur->wait_status) {
    cur->wait_status->exit_code = cur->exit_status;
    sema_up(&cur->wait_status->sema);
  }
  /* 2-2 Argument Passing - process_wait */

  /* 2-3. System Call - ROX */
  if (cur->executing_file) {
    lock_acquire(&file_system_lock);
    file_allow_write(cur->executing_file);
    file_close(cur->executing_file);
    cur->executing_file = NULL;
    lock_release(&file_system_lock);
  }
  /* 2-3. System Call - ROX */
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  lock_acquire(&file_system_lock);
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      lock_release(&file_system_lock);
      goto done; 
    }
  t->executing_file = file; /* 2-3. System Call - ROX */
  file_deny_write(file); /* 2-3. System Call - ROX */

  /* Read and verify executable header. */
  int bytes_read = file_read (file, &ehdr, sizeof ehdr);
  lock_release(&file_system_lock);
  if (bytes_read != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      lock_acquire(&file_system_lock);
      if (file_ofs < 0 || file_ofs > file_length (file)){
        lock_release(&file_system_lock);
        goto done;
      }
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr){
        lock_release(&file_system_lock);
        goto done;
      }
      lock_release(&file_system_lock);
      
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  /* 2-3. System Call - ROX */
  if (!success) {
    lock_acquire(&file_system_lock);
    if (t->executing_file) {
      file_allow_write (t->executing_file);
      file_close (t->executing_file);
      t->executing_file = NULL;
    } else if (file) {
      file_close (file);
    }
    lock_release(&file_system_lock);
  }
  /* 2-3. System Call - ROX */
  
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  lock_acquire(&file_system_lock);
  Elf32_Off file_length_ = (Elf32_Off) file_length (file);
  lock_release(&file_system_lock);
  if (phdr->p_offset > file_length_) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  /* 기존과 같은 sanity check */
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  struct thread *t = thread_current ();

  /* 더 이상 여기서는 file_read / palloc_get_page / install_page 를 하지 않음
     대신 페이지마다 vm_entry 를 만들어 SPT에 넣어두기만 함 */
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* 이번 페이지에서 실제로 파일에서 읽을 양 / zero-fill 양 계산 */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* vm_entry 하나 할당 */
      struct vm_entry *vme = malloc (sizeof *vme);
      if (vme == NULL)
        return false;

      vme->type       = VM_BIN;        /* 실행 파일 세그먼트 */
      vme->vaddr      = upage;         /* user 가 보게 될 가상주소 (page-aligned) */
      vme->writable   = writable;

      vme->file       = file;          /* 필요하면 file_reopen(file) 써도 됨 */
      vme->offset     = ofs;
      vme->read_bytes = page_read_bytes;
      vme->zero_bytes = page_zero_bytes;

      vme->swap_slot  = (size_t) -1;   /* 아직 swap에 안 나감 */
      vme->is_loaded  = false;         /* lazy loading이므로 아직 메모리에 없음 */

      /* SPT에 등록 (중복 / 실패시 메모리 정리 후 false) */
      if (!spt_insert (&t->spt, vme))
        {
          free (vme);
          return false;
        }

      /* 다음 페이지로 이동 */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage      += PGSIZE;
      ofs        += page_read_bytes;
    }
  return true;
}


/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  struct thread *t = thread_current ();
  uint8_t *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;

  struct vm_entry *vme = malloc (sizeof *vme);
  if (vme == NULL)
    return false;

  vme->type       = VM_ANON;       /* 익명 페이지 */
  vme->vaddr      = upage;         /* user 가 보게 될 가상주소 (page-aligned) */
  vme->writable   = true;
  vme->file       = NULL;          /* 익명 페이지이므로 파일 없음 */
  vme->offset     = 0;
  vme->read_bytes = 0;
  vme->zero_bytes = PGSIZE;
  vme->swap_slot  = (size_t) -1;   /* 아직 swap에 안 나감 */
  vme->is_loaded  = false;         /* lazy loading이므로 아직 메모리에 없음 */

  void *kpage = frame_alloc (PAL_USER | PAL_ZERO, vme);
  if (kpage == NULL)
  {
    free (vme);
    return false;
  }

  if (!install_page (upage, kpage, true)) 
  {
    frame_free (kpage);
    free (vme);
    return false; 
  }

  vme->is_loaded = true;

  if (!spt_insert (&t->spt, vme))
  {
    free (vme);
    return false;
  } 
  *esp = PHYS_BASE;
  return true;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/* 2-2 Argument Passing */
int fn_to_argv(char **argv, char *fn_copy) {
  int argc = 0;
  char *token;
  char *save_ptr;

  for (token = strtok_r(fn_copy, " ", &save_ptr); token != NULL;
       token = strtok_r(NULL, " ", &save_ptr)) {
    argv[argc++] = token;
  }
  return argc;
}

void stack_argv(char **argv, int argc, void **esp) {
  int i = 0;
  int length = 0;
  void *arg_addresses[argc];

  // Push arguments onto the stack in reverse order
  for (i = argc - 1; i >= 0; i--) {
    length = strlen(argv[i]);
    *esp = (uint8_t *)(*esp) - (length + 1);
    strlcpy(*esp, argv[i], length + 1);
    argv[i] = (char *)(*esp);
  }

  // Word-align the stack pointer
  uintptr_t esp_val = (uintptr_t)(*esp);
  // esp_val &= 0xfffffffc; // Align to 4 bytes
  // *esp = (void *)esp_val;
  size_t pad = esp_val % 4;
  if (pad) {
    *esp = (uint8_t *)(*esp) - pad;
    memset(*esp, 0, pad);
  }

  // Push null sentinel
  *esp = (uint8_t *)(*esp) - 4;
  *(uint32_t *)(*esp) = 0;

  // Push addresses of arguments
  for (i = argc - 1; i >= 0; i--) {
    *esp = (uint8_t *)(*esp) - 4;
    *(uint32_t *)(*esp) = (uint32_t)argv[i];
  }

  // Push argv (address of first argument)
  uint32_t argv0_addr = (uint32_t)(*esp);
  *esp = (uint8_t *)(*esp) - 4;
  *(uint32_t *)(*esp) = argv0_addr;

  // Push argc
  *esp = (uint8_t *)(*esp) - 4;
  *(uint32_t *)(*esp) = argc;

  // Push fake return address
  *esp = (uint8_t *)(*esp) - 4;
  *(uint32_t *)(*esp) = 0;
}
/* 2-2 Argument Passing */
