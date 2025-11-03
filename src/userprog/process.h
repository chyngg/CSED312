#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pid_t;

struct pcb {

    /* 2-3. System Call */
    pid_t pid;
    struct pcb * parent;
    struct list fd;
    struct list_elem pcb_elem;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
