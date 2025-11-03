#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
void sys_halt (void);
void sys_exit (int status);
pid_t sys_exec (const char *file);
int sys_wait (pid_t);
bool sys_create (const char *file, unsigned initial_size);
bool sys_remove (const char *file);
int sys_open (const char *file);
int sys_filesize (int fd);
int sys_read (int fd, void *buffer, unsigned length);
int sys_write (int fd, const void *buffer, unsigned length);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close (int fd);
void extract_arguments (struct intr_frame *f, int *syscall_argu, int argu_num);

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
  int syscall_argu[3] = {0, };

  switch(syscall_num){
    // void sys_halt (void) NO_RETURN
		case SYS_HALT:
			sys_halt ();
			break;

    // void sys_exit (int status) NO_RETURN
		case SYS_EXIT:
      extract_arguments(f, syscall_argu, 1);
			sys_exit (syscall_argu[0]);
			break;

    // pid_t sys_exec (const char *file)
		case SYS_EXEC:
      extract_arguments(f, syscall_argu, 1);
			sys_exec ((const char *) syscall_argu[0]);
			break;

    // int sys_wait (pid_t)
    case SYS_WAIT:
      extract_arguments(f, syscall_argu, 1);
			sys_wait ((pid_t) syscall_argu[0]);
			break;

    // bool sys_create (const char *file, unsigned initial_size)
    case SYS_CREATE:
      extract_arguments(f, syscall_argu, 2);
			sys_create ((const char *) syscall_argu[0], (unsigned) syscall_argu[1]);
			break;

    // bool sys_remove (const char *file)
    case SYS_REMOVE:
      extract_arguments(f, syscall_argu, 1);
			sys_remove ((const char *) syscall_argu[0]);
			break;

    // int sys_open (const char *file)
    case SYS_OPEN:
      extract_arguments(f, syscall_argu, 1);
      sys_open ((const char *) syscall_argu[0]);
			break;

    // int sys_filesize (int fd)
    case SYS_FILESIZE:
      extract_arguments(f, syscall_argu, 1);
      sys_filesize (syscall_argu[0]);
			break;

    // int sys_read (int fd, void *buffer, unsigned length)
    case SYS_READ:
      extract_arguments(f, syscall_argu, 3);
      sys_read (syscall_argu[0], (void *) syscall_argu[1], (unsigned) syscall_argu[2]);
			break;

    // int sys_write (int fd, const void *buffer, unsigned length)
    case SYS_WRITE:
      extract_arguments(f, syscall_argu, 3);
      sys_write (syscall_argu[0], (void *) syscall_argu[1], (unsigned) syscall_argu[2]);
			break;

    // void sys_seek (int fd, unsigned position)
    case SYS_SEEK:
      extract_arguments(f, syscall_argu, 2);
      sys_seek (syscall_argu[0], (unsigned) syscall_argu[1]);
			break;

    // unsigned sys_tell (int fd)
    case SYS_TELL:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      sys_tell (syscall_argu[0]);
			break;

    // void sys_close (int fd)
		case SYS_CLOSE:
      extract_arguments(f, syscall_argu, 1);
			break;
	}
}

void extract_arguments (struct intr_frame *f, int *syscall_argu, int argu_num){
  void *vaddr = f->esp + 4;
  for (int i = 0; i < argu_num; i++){
    syscall_argu[i] = *(int *)(vaddr);
    vaddr += 4;
  }
}

void sys_halt (void){
  shutdown_power_off();
}

void sys_exit (int status){
  thread_current()->status = status;
  thread_exit();
}

pid_t sys_exec (const char *file){
  
}

int sys_wait (pid_t){
  
}

bool sys_create (const char *file, unsigned initial_size){
  
}

bool sys_remove (const char *file){

}

int sys_open (const char *file){

}

int sys_filesize (int fd){
  
}

int sys_read (int fd, void *buffer, unsigned length){
  
}

int sys_write (int fd, const void *buffer, unsigned length){
  
}

void sys_seek (int fd, unsigned position){
  
}
unsigned sys_tell (int fd){

}

void sys_close (int fd){
  
}