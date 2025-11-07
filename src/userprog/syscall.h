#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int pid_t;

static struct lock file_system_lock;

void syscall_init (void);


#endif /* userprog/syscall.h */
