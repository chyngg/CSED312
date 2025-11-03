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
  // printf ("system call!\n");
  // thread_exit ();
  if(!is_user_vaddr(f->esp)){
    thread_exit (); // ?
  }

  int syscall_num = *(int *)(f->esp);

  switch(syscall_num){
    // void sys_halt (void) NO_RETURN
		case SYS_HALT:
			sys_halt ();
			break;
    // void sys_exit (int status) NO_RETURN
		case SYS_EXIT:
      int status = 
			sys_exit (status);
			break;
    // pid_t sys_exec (const char *file)
		case SYS_EXEC:
			sys_exec (file);
			break;
    // int sys_wait (pid_t)
    case SYS_WAIT:
			sys_wait (pid);
			break;
    // bool sys_create (const char *file, unsigned initial_size)
    case SYS_CREATE:
			sys_create (file, initial_size);
			break;
    // bool sys_remove (const char *file)
    case SYS_REMOVE:
			sys_remove (file);
			break;
    // int sys_open (const char *file)
    case SYS_OPEN:
      sys_open (file);
			break;
    // int sys_filesize (int fd)
    case SYS_FILESIZE:
      sys_filesize (fd);
			break;
    // int sys_read (int fd, void *buffer, unsigned length)
    case SYS_READ:
      sys_read (fd, buffer, length);
			break;
    // int sys_write (int fd, const void *buffer, unsigned length)
    case SYS_WRITE:
      sys_write (fd, buffer, length);
			break;
    // void sys_seek (int fd, unsigned position)
    case SYS_SEEK:
      sys_seek (fd, position);
			break;
    // unsigned sys_tell (int fd)
    case SYS_TELL:
      sys_tell (fd);
			break;
    // void sys_close (int fd)
		case SYS_CLOSE:
      sys_close (fd);
			break;
	}
}
