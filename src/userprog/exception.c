#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

extern struct lock file_system_lock;

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* 유저 코드에서 난 예외 → 유저 프로세스만 죽임 */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_current()->exit_status = -1;
      thread_exit ();
      NOT_REACHED ();
      /* NOT_REACHED */

    case SEL_KCSEG:
      /* 커널 코드에서 난 예외 → 커널 버그이므로 그냥 패닉 */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
              f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Unexpected interrupt in unknown segment");
    }
}


/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  intr_enable ();
  page_fault_cnt++;

  not_present = (f->error_code & PF_P) == 0;
  write       = (f->error_code & PF_W) != 0;
  user        = (f->error_code & PF_U) != 0;

  struct thread *t = thread_current ();

  /* 1) write to read-only page */
  if (!not_present) 
    {
      if (user || is_user_vaddr (fault_addr))
        {
          if (lock_held_by_current_thread (&file_system_lock))
            lock_release (&file_system_lock);
          t->exit_status = -1;
          thread_exit ();
        }
      else 
        {
          kill (f);
        }
      NOT_REACHED ();
    }

  /* 2) user 영역이 아니거나 NULL 접근 */
  if (!is_user_vaddr (fault_addr) || fault_addr == NULL)
    {
      if (user)
        {
          t->exit_status = -1;
          thread_exit ();
        }
      else 
        {
          kill (f);
        }
      NOT_REACHED ();
    }

  /* 3) SPT에서 vm_entry 찾기 */
  struct vm_entry *vme = spt_find (&t->spt, fault_addr);
  if (vme != NULL)
    {
      if (!handle_fault (vme))
        {
          if (user)
            {
              t->exit_status = -1;
              thread_exit ();
            }
          else
            {
              kill (f);
            }
        }
      return;
    }

  /* 4) stack growth 체크 */
  if (user)
    {
      void *esp = f->esp;
      if (check_stack_boundary (fault_addr, esp))
        {
          void *upage = pg_round_down (fault_addr);
          if (!expand_stack (upage))
            {
              t->exit_status = -1;
              thread_exit ();
            }
          return;
        }

      /* stack growth도 아니면 잘못된 접근 */
      t->exit_status = -1;
      thread_exit ();
    }
  else
  {
    t->exit_status = -1;
    thread_exit ();
  }
}


