#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* 2-3. System Call */
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct lock file_system_lock;

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
bool is_mapped_user_vaddr (const void *vaddr, size_t size);
void extract_arguments (struct intr_frame *f, int *syscall_argu, int argu_num);
struct file * access_file (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_system_lock); 
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf ("system call!\n");
  // thread_exit ();
  if(!is_mapped_user_vaddr(f->esp, sizeof(int))){
    sys_exit (-1);
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
			f->eax = sys_exec ((const char *) syscall_argu[0]);
			break;

    // int sys_wait (pid_t)
    case SYS_WAIT:
      extract_arguments(f, syscall_argu, 1);
			f->eax = sys_wait ((pid_t) syscall_argu[0]);
			break;

    // bool sys_create (const char *file, unsigned initial_size)
    case SYS_CREATE:
      extract_arguments(f, syscall_argu, 2);
			f->eax = sys_create ((const char *) syscall_argu[0], (unsigned) syscall_argu[1]);
			break;

    // bool sys_remove (const char *file)
    case SYS_REMOVE:
      extract_arguments(f, syscall_argu, 1);
			f->eax = sys_remove ((const char *) syscall_argu[0]);
			break;

    // int sys_open (const char *file)
    case SYS_OPEN:
      extract_arguments(f, syscall_argu, 1);
      f->eax = sys_open ((const char *) syscall_argu[0]);
			break;

    // int sys_filesize (int fd)
    case SYS_FILESIZE:
      extract_arguments(f, syscall_argu, 1);
      f->eax = sys_filesize (syscall_argu[0]);
			break;

    // int sys_read (int fd, void *buffer, unsigned length)
    case SYS_READ:
      extract_arguments(f, syscall_argu, 3);
      f->eax = sys_read (syscall_argu[0], (void *) syscall_argu[1], (unsigned) syscall_argu[2]);
			break;

    // int sys_write (int fd, const void *buffer, unsigned length)
    case SYS_WRITE:
      extract_arguments(f, syscall_argu, 3);
      f->eax = sys_write (syscall_argu[0], (void *) syscall_argu[1], (unsigned) syscall_argu[2]);
			break;

    // void sys_seek (int fd, unsigned position)
    case SYS_SEEK:
      extract_arguments(f, syscall_argu, 2);
      sys_seek (syscall_argu[0], (unsigned) syscall_argu[1]);
			break;

    // unsigned sys_tell (int fd)
    case SYS_TELL:
      extract_arguments(f, syscall_argu, 1);
      f->eax = sys_tell (syscall_argu[0]);
			break;

    // void sys_close (int fd)
		case SYS_CLOSE:
      extract_arguments(f, syscall_argu, 1);
      sys_close (syscall_argu[0]);
			break;
	}
}

bool is_mapped_user_vaddr (const void *vaddr, size_t size){
  if(vaddr == NULL){
    return false;
  }

  const uint8_t *temp = (const uint8_t *)vaddr;
  const uint8_t *end = temp + size;

  while (temp < end) {
    if (!is_user_vaddr(temp)){
      return false;
    }

    void *kpage = pagedir_get_page(thread_current()->pagedir, temp);

    if (kpage == NULL){
      return false;
    }

    temp = (const uint8_t *)pg_round_down((void *)temp) + PGSIZE; // 다음 페이지로
  }

  return true;

}

void extract_arguments (struct intr_frame *f, int *syscall_argu, int argu_num){
  void *vaddr = f->esp + 4;
  if(!is_mapped_user_vaddr(vaddr, argu_num * (int)sizeof(int))){
    sys_exit (-1);
  }
  for (int i = 0; i < argu_num; i++){
    syscall_argu[i] = *(int *)(vaddr);
    vaddr += 4;
  }
}

bool is_valid_str(const char *str) {
  if (str == NULL) {
    return false;
  }
  const char *temp = str;
  for (;;) {
    if(!is_mapped_user_vaddr(temp, 1)){
      return false;
    }
    if (*temp == '\0'){
      break;
    }
    temp++;
  }
  
  return true;
}

struct file * access_file (int fd){
  if(2 <= fd && fd < thread_current()->fd_next){
    return thread_current()->fd_list[fd];
  }
  else{
    return NULL; 
  }
}

void sys_halt (void){
  shutdown_power_off();
}

void sys_exit (int status){
  thread_current()->exit_status = status;
  printf ("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}

pid_t sys_exec (const char *file){
  if(!is_valid_str(file)){
    sys_exit (-1);
  }
  pid_t pid = process_execute(file);
  struct thread * child = get_child_process(pid);
  
  if (!child || !child->loaded) {
    return -1;
  }
  return pid;
}

int sys_wait (pid_t pid){
  int exit_status = process_wait(pid);
  return exit_status;
}

bool sys_create (const char *file, unsigned initial_size){
  if(!is_valid_str(file)){
    sys_exit (-1);
  }
  lock_acquire(&file_system_lock);
  bool success = filesys_create (file, initial_size);
  lock_release(&file_system_lock);
  return success;
}

bool sys_remove (const char *file){
  if(!is_valid_str(file)){
    sys_exit (-1);
  }
  lock_acquire(&file_system_lock);
  bool success = filesys_remove (file);
  lock_release(&file_system_lock);
  return success;
}

int sys_open (const char *file){
  if(!is_valid_str(file)){
    sys_exit (-1);
  }
  lock_acquire(&file_system_lock);
  struct file * f = filesys_open(file);
  if(!f){
    lock_release(&file_system_lock);
    return -1;
  }
  lock_release(&file_system_lock);
  int fd = thread_current()->fd_next;
  thread_current()->fd_list[fd] = f;
  thread_current()->fd_next++;
  return fd;
}

int sys_filesize (int fd){
  lock_acquire(&file_system_lock);
  struct file * f = access_file (fd);
  if(!f){
    lock_release(&file_system_lock);
    return -1;
  }
  lock_release(&file_system_lock);
  return file_length(f);
}

int sys_read (int fd, void *buffer, unsigned length){
  int length_read = 0;
  lock_acquire(&file_system_lock);
  if(fd == 0){ // stdin 
    for (int i = 0; i < length;i++){
      ((char*)buffer)[i]=input_getc();
      if(((char*)buffer)[i] == '\0')
      {
        break;
      }
      length_read = i;
    }
  }
  else if(fd == 1){
    lock_release(&file_system_lock);
    return -1;
  }
  else{
    struct file * f = access_file (fd);
    if(!f){
      lock_release(&file_system_lock);
      return -1;
    }
    length_read = file_read (f, buffer, length);
  }
  lock_release(&file_system_lock);
  return length_read;
}

int sys_write (int fd, const void *buffer, unsigned length){
  int length_write = 0;
  lock_acquire(&file_system_lock);
  if(fd == 0){ // stdin 
    lock_release(&file_system_lock);
    return -1;
  }
  else if(fd == 1){
    putbuf(buffer, length);
    length_write = length;
  }
  else{
    struct file * f = access_file (fd);
    if(!f){
      lock_release(&file_system_lock);
      return -1;
    }
    length_write = file_write (f, buffer, length);
  }
  lock_release(&file_system_lock);
  return length_write;
}

void sys_seek (int fd, unsigned position){
  lock_acquire(&file_system_lock);
  struct file * f = access_file (fd);
  if(!f){
    lock_release(&file_system_lock);
    return;
  }
  file_seek (f, position);
  lock_release(&file_system_lock);
  return;
}

unsigned sys_tell (int fd){
  lock_acquire(&file_system_lock);
  struct file * f = access_file (fd);
  if(!f){
    lock_release(&file_system_lock);
    return -1; // ?
  }
  unsigned position = file_tell (f);
  lock_release(&file_system_lock);
  return position;
}

void sys_close (int fd){
  lock_acquire(&file_system_lock);
  struct file * f = access_file (fd);
  if(!f){
    lock_release(&file_system_lock);
    return;
  }
  file_close(fd);
  thread_current()->fd_list[fd] = NULL;
  lock_release(&file_system_lock);
}
/* 2-3. System Call */
