#include "frame.h"
#include "list.h"
#include "page.h"
#include "swap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

struct frame
  {
    struct list_elem elem;
    struct thread *thread;
    void *upage;
    void *kpage;
  };

static struct list frame_table = LIST_INITIALIZER(frame_table);
static struct list locked_frame_list = LIST_INITIALIZER(locked_frame_list);
static struct lock frame_table_lock;

static void frame_table_update (void);
static void lock_frame (struct frame *);
static void unlock_frame (struct frame *);
static struct frame* find_frame (const void *);
static struct frame* find_frame_with_physical_address (struct list *, const void *);

void
frame_table_init (void)
{
  lock_init (&frame_table_lock);
}

static void
frame_table_update (void)
{
  if (list_empty (&frame_table))
    return;

  lock_acquire (&frame_table_lock);

  struct list_elem *cursor = list_begin (&frame_table);

  while (cursor != list_end (&frame_table))
    {
      struct frame *f = list_entry (cursor, struct frame, elem);
      if (f->thread->pagedir != NULL && pagedir_is_accessed (f->thread->pagedir, f->upage))
        {
          struct list_elem *prev = cursor;
          cursor = list_remove (prev);
          list_push_front (&frame_table, prev);
        }
      else
        cursor = list_next (cursor);
    }

  lock_release (&frame_table_lock);
}

void *
frame_table_set_frame (void *upage)
{
  void *kpage = palloc_get_page (PAL_USER);
  struct frame *f;

  if (kpage == NULL)
    {
      frame_table_update ();

      lock_acquire (&frame_table_lock);
      f = list_entry (list_back (&frame_table), struct frame, elem);
      list_remove (&f->elem);
      lock_release (&frame_table_lock);

      struct page *p = page_table_lookup (&f->thread->page_table, f->upage);
      switch (p->segment)
        {
          case SEG_CODE:
          case SEG_STACK:
            if (p->writable && pagedir_is_dirty (f->thread->pagedir, f->upage));
              p->position = swap_table_swap_in (f->thread->tid, f->upage, f->kpage);
            break;
          case SEG_MAPPING:
            if (pagedir_is_dirty (f->thread->pagedir, f->upage))
              {
                file_seek (p->file, p->position);
                file_write (p->file, f->kpage, PGSIZE);
              }
            break;
        }

      pagedir_clear_page (f->thread->pagedir, f->upage);
    }
  else
    {
      f = malloc (sizeof (struct frame));
      f->kpage = kpage;
    }

  f->thread = thread_current ();
  f->upage = upage;

  lock_acquire (&frame_table_lock);
  list_push_front (&frame_table, &f->elem);
  lock_release (&frame_table_lock);

  return f->kpage;
}

void *
frame_table_get_frame (void *upage)
{
  lock_acquire (&frame_table_lock);
  struct frame *f = find_frame (upage);
  lock_release (&frame_table_lock);
  return f != NULL ? f->kpage : NULL;
}

void
frame_table_clear_frame (void *upage)
{
  lock_acquire (&frame_table_lock);
  struct frame *f = find_frame (upage);
  if (f != NULL)
    {
      list_remove (&f->elem);
      free (f);
    }
  lock_release (&frame_table_lock);
}

void
frame_table_lock_frame (void *kpage)
{
  struct frame *f = find_frame_with_physical_address (&frame_table, kpage);
  if (f)
    lock_frame (f);
}

void
frame_table_unlock_frame (void *kpage)
{
  struct frame *f = find_frame_with_physical_address (&locked_frame_list, kpage);
  if (f)
    unlock_frame (f);
}

static void
lock_frame (struct frame *f)
{
  lock_acquire (&frame_table_lock);
  list_remove (&f->elem);
  list_push_back (&locked_frame_list, &f->elem);
  lock_release (&frame_table_lock);
}

static void
unlock_frame (struct frame *f)
{
  lock_acquire (&frame_table_lock);
  list_remove (&f->elem);
  list_push_front (&frame_table, &f->elem);
  lock_release (&frame_table_lock);
}

static struct frame *
find_frame (const void *upage)
{
  for (struct list_elem *cursor = list_begin (&frame_table); cursor != list_end (&frame_table); cursor = list_next (cursor))
    {
      struct frame *f = list_entry (cursor, struct frame, elem);
      if (f->thread == thread_current () && f->upage == upage)
        return f;
    }

  return NULL;
}

static struct frame*
find_frame_with_physical_address (struct list *list, const void *address)
{
  for (struct list_elem *cursor = list_begin (list); cursor != list_end (list); cursor = list_next (cursor))
    {
      struct frame *f = list_entry (cursor, struct frame, elem);
      if (f->kpage == address)
        return f;
    }

  return NULL;
}