#include "userprog/syscall.h"
#include "userprog/exception.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "devices/input.h"

#define STDOUT_BUFFER_SIZE 512

static struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);

static void halt (void);
static tid_t exec (void *stack);
static int wait (void *stack);
static bool create (void *stack);
static bool remove (void *stack);
static int open (void *stack);
static int filesize (void *stack);
static int read (void *stack);
static int write (void *stack);
static void seek (void *stack);
static unsigned tell (void *stack);
static void close (void *stack);

static void pop_stack (void **esp, void *value, size_t size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int number = 0;
  pop_stack (&f->esp, &number, sizeof number);

  switch (number)
    {
      case SYS_HALT:
        halt ();
        break;
      case SYS_EXIT:
        {
          int status;
          pop_stack (&f->esp, &status, sizeof status);

          exit (status);
        }
        break;
      case SYS_EXEC:
        f->eax = exec (f->esp);
        break;
      case SYS_WAIT:
        f->eax = wait (f->esp);
        break;
      case SYS_CREATE:
        f->eax = create (f->esp);
        break;
      case SYS_REMOVE:
        f->eax = remove (f->esp);
        break;
      case SYS_OPEN:
        f->eax = open (f->esp);
        break;
      case SYS_FILESIZE:
        f->eax = filesize (f->esp);
        break;
      case SYS_READ:
        f->eax = read (f->esp);
        break;
      case SYS_WRITE:
        f->eax = write (f->esp);
        break;
      case SYS_SEEK:
        seek (f->esp);
        break;
      case SYS_TELL:
        f->eax = tell (f->esp);
        break;
      case SYS_CLOSE:
        close (f->esp);
        break;
      default:
        exit (-1);
        break;
    }
}

static void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  struct process *process = process_current ();
  if (process != NULL)
    process->exit_status = status;

  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

static tid_t
exec (void *stack)
{
  char *command;
  pop_stack (&stack, &command, sizeof command);

  if (is_user_vaddr (command))
    return process_execute (command);

  return TID_ERROR;
}

static int
wait (void *stack)
{
  tid_t tid;
  pop_stack (&stack, &tid, sizeof tid);

  return process_wait (tid);
}

static bool
create (void *stack)
{
  char *filename;
  pop_stack (&stack, &filename, sizeof filename);

  unsigned initial_size;
  pop_stack (&stack, &initial_size, sizeof initial_size);

  if (!filename)
    exit (-1);
  if (!is_user_vaddr (filename))
    return false;

  lock_acquire (&filesys_lock);
  bool result = filesys_create (filename, initial_size);
  lock_release (&filesys_lock);
  return result;
}

static bool
remove (void *stack)
{
  char *filename;
  pop_stack (&stack, &filename, sizeof filename);

  if (!is_user_vaddr (filename))
    return false;
  
  lock_acquire (&filesys_lock);
  bool result = filesys_remove (filename);
  lock_release (&filesys_lock);
  return result;
}

static int
open (void *stack)
{
  char *filename;
  pop_stack (&stack, &filename, sizeof filename);

  struct process *process = process_current ();

  if (filename && is_user_vaddr (filename))
    {
      int fd = STDOUT_FILENO + 1;
      while (fd < NOFILE)
        {
          if (!process->open_files[fd])
            break;
          else
            ++fd;
        }
  
      if (fd < NOFILE)
        {
          lock_acquire (&filesys_lock);
          process->open_files[fd] = filesys_open (filename);
          lock_release (&filesys_lock);
          if (process->open_files[fd])
            return fd;
        }
    }

  return -1;
}

static int
filesize (void *stack)
{
  int fd;
  pop_stack (&stack, &fd, sizeof fd);

  struct process *process = process_current ();

  if (fd < 0 || fd >= NOFILE)
    goto error;
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO)
    goto error;

  if (process->open_files[fd])
    return file_length (process->open_files[fd]);

error:
  return -1;
}

static int
read (void *stack)
{
  int fd;
  pop_stack (&stack, &fd, sizeof fd);

  void *buffer;
  pop_stack (&stack, &buffer, sizeof buffer);

  unsigned length;
  pop_stack (&stack, &length, sizeof length);

  struct process *process = process_current ();

  if (fd < 0 || fd >= NOFILE)
    goto error;
  if (!(is_user_vaddr (buffer) && is_user_vaddr (buffer + length)))
    exit (-1);

  if (fd == STDIN_FILENO)
    {
      unsigned read_bytes = 0;
      while (read_bytes < length)
        {
          uint8_t data = input_getc ();
          memcpy ((uint8_t*)buffer + read_bytes, &data, sizeof data);
          read_bytes += sizeof (uint8_t);
        }
      return length;
    }
  else if (fd != STDOUT_FILENO && process->open_files[fd])
    return file_read (process->open_files[fd], buffer, length);

error:
  return -1;
}

static int
write (void *stack)
{
  int fd;
  pop_stack (&stack, &fd, sizeof fd);

  void *data;
  pop_stack (&stack, &data, sizeof data);

  unsigned length;
  pop_stack (&stack, &length, sizeof length);

  struct process *process = process_current ();

  if (fd < 0 || fd >= NOFILE)
    goto error;
  if (!(is_user_vaddr (data) && is_user_vaddr (data + length)))
    goto error;

  if (fd == STDOUT_FILENO)
    {
      int remaining = length;
      int offset = 0;

      while (remaining > STDOUT_BUFFER_SIZE)
        {
          putbuf (data + offset, STDOUT_BUFFER_SIZE);
          remaining -= STDOUT_BUFFER_SIZE;
          offset += STDOUT_BUFFER_SIZE;
        }
      
      if (remaining > 0)
        putbuf (data + offset, remaining);

      return length;
    }
  else if (fd != STDIN_FILENO && process->open_files[fd])
    return file_write (process->open_files[fd], data, length);

error:
  return -1;
}

static void
seek (void *stack)
{
  int fd;
  pop_stack (&stack, &fd, sizeof fd);

  unsigned position;
  pop_stack (&stack, &position, sizeof position);

  struct process *process = process_current ();

  if (fd >= 0 && fd < NOFILE)
    file_seek (process->open_files[fd], position);
}

static unsigned
tell (void *stack)
{
  int fd;
  pop_stack (&stack, &fd, sizeof fd);

  struct process *process = process_current ();

  if (fd >= 0 && fd < NOFILE)
    return file_tell (process->open_files[fd]);
  else
    return 0;
}

static void
close (void *stack)
{
  int fd;
  pop_stack (&stack, &fd, sizeof fd);

  struct process *process = process_current ();

  if (fd >= 0 && fd < NOFILE)
    {
      file_close (process->open_files[fd]);
      process->open_files[fd] = NULL;
    }
}

static void
pop_stack (void **esp, void *value, size_t size)
{
  memcpy (value, *esp, size);
  *esp += size;
  if (!is_user_vaddr (*esp))
    exit (-1);
}
