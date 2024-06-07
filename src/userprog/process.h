#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define NOFILE 128

struct process
  {
    tid_t tid;

    struct semaphore running;
    int exit_status;

    tid_t parent;

    struct file *executable;
    struct file *open_files[NOFILE];

    struct list_elem elem;
  };

void process_init (void);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process *process_current (void);
bool process_has_child (tid_t);

#endif /* userprog/process.h */
