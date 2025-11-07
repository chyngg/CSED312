#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* 2-3. System Call */
#include "threads/synch.h"
#include "threads/vaddr.h"

struct lock file_system_lock;

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

static void copy_in (void *dst_, const void *usrc_, size_t size) {
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;
  for (size_t i = 0; i < size; i++) {
    if (!is_user_vaddr(usrc) ||
        pagedir_get_page(thread_current()->pagedir, (void *)usrc) == NULL)
      sys_exit(-1);
    dst[i] = *usrc++;
  }
}

static char *copy_in_string (const char *ustr) {
  if (ustr == NULL) sys_exit(-1);
  char *kbuf = palloc_get_page (0);
  if (!kbuf) sys_exit(-1);
  size_t i = 0;
  while (i < PGSIZE - 1) {
    if (!is_user_vaddr(ustr + i) ||
        pagedir_get_page(thread_current()->pagedir, (void *)(ustr + i)) == NULL) {
      palloc_free_page(kbuf);
      sys_exit(-1);
    }
    char c = *(ustr + i);
    kbuf[i++] = c;
    if (c == '\0') break;
  }
  if (i == PGSIZE - 1) { palloc_free_page(kbuf); sys_exit(-1); }
  return kbuf; // palloc 페이지(커널 메모리)
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

  char *kfile = copy_in_string(file);
  pid_t pid = process_execute(kfile);
  palloc_free_page(kfile);
  if (pid == TID_ERROR){
    return -1;
  }

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
  if(*file == '\0'){
    return false;
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
  if(*file == '\0'){
    return -1;
  }

  lock_acquire(&file_system_lock);
  struct file * f = filesys_open (file);
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
  int filesize = file_length(f);
  lock_release(&file_system_lock);
  return filesize;
}

int sys_read (int fd, void *buffer, unsigned length){
  if((length != 0) && (!is_mapped_user_vaddr(buffer, length))){
    sys_exit (-1);
  }

  int length_read = 0;
  
  if(fd == 0){ // stdin 
    for (int i = 0; i < length;i++){
      ((char*)buffer)[i]=input_getc();
      length_read ++;
    }
  }
  else if(fd == 1){
    return -1;
  }
  else{
    lock_acquire(&file_system_lock);
    struct file * f = access_file (fd);
    if(!f){
      lock_release(&file_system_lock);
      return -1;
    }
    length_read = file_read (f, buffer, length);
    lock_release(&file_system_lock);
  }

  return length_read;
}

int sys_write (int fd, const void *buffer, unsigned length){
  if((length != 0) && (!is_mapped_user_vaddr(buffer, length))){
    sys_exit (-1);
  }
  
  int length_write = 0;
  
  if(fd == 0){ // stdin 
    return -1;
  }
  else if(fd == 1){
    putbuf(buffer, length);
    length_write = length;
  }
  else{
    lock_acquire(&file_system_lock);
    struct file * f = access_file (fd);
    if(!f){
      lock_release(&file_system_lock);
      return -1;
    }
    length_write = file_write (f, buffer, length);
    lock_release(&file_system_lock);
  }
  
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
  file_close (f);
  lock_release(&file_system_lock);
  thread_current()->fd_list[fd] = NULL;
}
/* 2-3. System Call */
