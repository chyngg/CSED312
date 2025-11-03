#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

/* 2-1 Process Termination Message */
void sys_exit (int exit_code)
{
  struct thread *cur = thread_current ();
  cur->exit_code = exit_code;
  printf ("%s: exit(%d)\n", thread_name(), exit_code);
  thread_exit ();
}
