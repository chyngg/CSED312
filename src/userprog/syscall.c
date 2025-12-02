#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <round.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* 2-3. System Call */
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "userprog/pagedir.h"

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
mapid_t sys_mmap (int fd, void *addr);
void sys_munmap (mapid_t mapid);
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
  check_address (f->esp);

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

    // mapid_t sys_mmap (int fd, void *addr)
    case SYS_MMAP:
			extract_arguments(f, syscall_argu, 2);
      f->eax = sys_mmap (syscall_argu[0], (void *) syscall_argu[1]);
			break;

    // void sys_munmap (mapid_t mapid)
		case SYS_MUNMAP:
			extract_arguments(f, syscall_argu, 1);
			sys_munmap ((mapid_t) syscall_argu[0]);
			break;
	}
}

void check_address (void *addr) {
  if (addr == NULL || !is_user_vaddr(addr)) {
    sys_exit (-1);
  }

  struct thread *cur = thread_current ();
  struct vm_entry *vme = spt_find (&cur->spt, addr);

  if (vme == NULL) {
    sys_exit (-1);
  }

  if (!vme->is_loaded) {
    if (!handle_fault (vme)) {
      sys_exit (-1);
    }
  }
}

bool is_mapped_user_vaddr (const void *vaddr, size_t size){
  if(vaddr == NULL){
    return false;
  }

  void *cur_addr = pg_round_down(vaddr);
  void *end_addr = pg_round_down((uint8_t *)vaddr + size - 1);

  struct thread *t = thread_current ();
  while (cur_addr <= end_addr) {
    if (!is_user_vaddr(cur_addr)){
      return false;
    }

    struct vm_entry *vme = spt_find (&t->spt, cur_addr);
    if (vme == NULL){
      return false;
    }
    cur_addr += PGSIZE;
  } 

  return true;

  /*
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
  */
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

/* 3 Virtual Memory - File memory mapping */
mapid_t sys_mmap (int fd, void *addr) {
  struct thread *t = thread_current ();

  if (fd == 0 || fd == 1 || addr == NULL || addr != pg_round_down (addr))
    return MAP_FAILED;

  lock_acquire (&file_system_lock);
  struct file *orig = access_file (fd);
  off_t length = orig ? file_length (orig) : 0;
  struct file *file = orig ? file_reopen (orig) : NULL;
  lock_release (&file_system_lock);

  if (file == NULL || length <= 0)
    {
      if (file != NULL)
        {
          lock_acquire (&file_system_lock);
          file_close (file);
          lock_release (&file_system_lock);
        }
      return MAP_FAILED;
    }

  size_t page_cnt = DIV_ROUND_UP ((size_t) length, PGSIZE);
  void *cur = addr;
  for (size_t i = 0; i < page_cnt; i++, cur += PGSIZE)
    {
      if (spt_find (&t->spt, cur) != NULL
          || !is_user_vaddr (cur)
          || !is_user_vaddr ((uint8_t *) cur + PGSIZE - 1))
        {
          lock_acquire (&file_system_lock);
          file_close (file);
          lock_release (&file_system_lock);
          return MAP_FAILED;
        }
    }

  struct mmap_entry *m = malloc (sizeof *m);
  if (m == NULL)
    {
      lock_acquire (&file_system_lock);
      file_close (file);
      lock_release (&file_system_lock);
      return MAP_FAILED;
    }

  m->id = t->next_mapid++;
  m->file = file;
  m->base = addr;
  m->length = length;
  m->page_cnt = page_cnt;
  list_push_back (&t->mmap_table, &m->elem);

  cur = addr;
  off_t ofs = 0;
  size_t remain = length;
  for (size_t i = 0; i < page_cnt; i++, cur += PGSIZE)
    {
      size_t page_read_bytes = remain < PGSIZE ? remain : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      struct vm_entry *vme = malloc (sizeof *vme);
      if (vme == NULL)
        {
          sys_munmap (m->id);
          return MAP_FAILED;
        }

      vme->type = VM_FILE;
      vme->vaddr = cur;
      vme->writable = true;
      vme->file = file;
      vme->offset = ofs;
      vme->read_bytes = page_read_bytes;
      vme->zero_bytes = page_zero_bytes;
      vme->swap_slot = (size_t) -1;
      vme->is_loaded = false;

      if (!spt_insert (&t->spt, vme))
        {
          free (vme);
          sys_munmap (m->id);
          return MAP_FAILED;
        }

      remain -= page_read_bytes;
      ofs += page_read_bytes;
    }

  return m->id;
}

void sys_munmap (mapid_t id) {
  struct thread *t = thread_current ();
  struct mmap_entry *target = NULL;

  struct list_elem *e;
  for (e = list_begin (&t->mmap_table); e != list_end (&t->mmap_table);
       e = list_next (e))
    {
      struct mmap_entry *m = list_entry (e, struct mmap_entry, elem);
      if (m->id == id)
        {
          target = m;
          break;
        }
    }

  if (target == NULL)
    return;

  lock_acquire (&file_system_lock);
  void *cur = target->base;
  for (size_t i = 0; i < target->page_cnt; i++, cur += PGSIZE)
    {
      struct vm_entry *vme = spt_find (&t->spt, cur);
      if (vme == NULL)
        continue;

      if (vme->is_loaded)
        {
          void *kaddr = pagedir_get_page (t->pagedir, vme->vaddr);
          if (kaddr != NULL)
            {
              if (pagedir_is_dirty (t->pagedir, vme->vaddr))
                file_write_at (target->file, kaddr, vme->read_bytes,
                               vme->offset);
              pagedir_clear_page (t->pagedir, vme->vaddr);
              frame_free (kaddr);
            }
        }

      spt_remove (&t->spt, vme);
    }

  file_close (target->file);
  lock_release (&file_system_lock);
  list_remove (&target->elem);
  free (target);
}
/* 3 Virtual Memory - File memory mapping */
