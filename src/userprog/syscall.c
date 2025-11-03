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

  switch(syscall_num){
    // void sys_halt (void) NO_RETURN
		case SYS_HALT:
			sys_halt ();
			break;

    // void sys_exit (int status) NO_RETURN
		case SYS_EXIT:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      int status = syscall_argu[0];
			sys_exit (status);
			break;

    // pid_t sys_exec (const char *file)
		case SYS_EXEC:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      const char * file = (const char *) syscall_argu[0];
			sys_exec (file);
			break;

    // int sys_wait (pid_t)
    case SYS_WAIT:
     int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      pid_t pid = (pid_t) syscall_argu[0];
			sys_wait (pid);
			break;

    // bool sys_create (const char *file, unsigned initial_size)
    case SYS_CREATE:
      int syscall_argu[2] = {0, };
      extract_arguments(f, syscall_argu, 2);
      const char * file = (const char *) syscall_argu[0];
      unsigned initial_size = (unsigned) syscall_argu[1];
			sys_create (file, initial_size);
			break;

    // bool sys_remove (const char *file)
    case SYS_REMOVE:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      const char * file = (const char *) syscall_argu[0];
			sys_remove (file);
			break;

    // int sys_open (const char *file)
    case SYS_OPEN:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      const char * file = (const char *) syscall_argu[0];
      sys_open (file);
			break;

    // int sys_filesize (int fd)
    case SYS_FILESIZE:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      int fd = syscall_argu[0];
      sys_filesize (fd);
			break;

    // int sys_read (int fd, void *buffer, unsigned length)
    case SYS_READ:
      int syscall_argu[3] = {0, };
      extract_arguments(f, syscall_argu, 3);
      int fd = syscall_argu[0];
      void * buffer = (void *) syscall_argu[1];
      unsigned length = (unsigned) syscall_argu[2];
      sys_read (fd, buffer, length);
			break;

    // int sys_write (int fd, const void *buffer, unsigned length)
    case SYS_WRITE:
      int syscall_argu[3] = {0, };
      extract_arguments(f, syscall_argu, 3);
      int fd = syscall_argu[0];
      void * buffer = (void *) syscall_argu[1];
      unsigned length = (unsigned) syscall_argu[2];
      sys_write (fd, buffer, length);
			break;

    // void sys_seek (int fd, unsigned position)
    case SYS_SEEK:
      int syscall_argu[2] = {0, };
      extract_arguments(f, syscall_argu, 2);
      int fd = syscall_argu[0];
      unsigned position = (unsigned) syscall_argu[1];
      sys_seek (fd, position);
			break;

    // unsigned sys_tell (int fd)
    case SYS_TELL:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      int fd = syscall_argu[0];
      sys_tell (fd);
			break;

    // void sys_close (int fd)
		case SYS_CLOSE:
      int syscall_argu[1] = {0, };
      extract_arguments(f, syscall_argu, 1);
      int fd = syscall_argu[0];
      sys_close (fd);
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