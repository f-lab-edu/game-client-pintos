#include "frame.h"
#include "list.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"

struct frame
  {
    struct list_elem elem;
    void *page;
  };

static struct list frame_table;
static struct lock frame_lock;

static struct frame* find_frame (const void *);

void
frame_table_init (void)
{
  list_init (&frame_table);
  lock_init (&frame_lock);
}

void *
frame_table_get_frame ()
{
  void *page = palloc_get_page (PAL_USER);
  if (page == NULL)
    {
      // TODO: swap page.
      PANIC ("Unable to allocate frame.");
    }
  struct frame *f = malloc (sizeof (struct frame));
  f->page = page;
  lock_acquire (&frame_lock);
  list_push_front (&frame_table, &f->elem);
  lock_release (&frame_lock);
  return f->page;
}

void
frame_table_free_frame (void *page)
{
  lock_acquire (&frame_lock);
  struct frame *f = find_frame (page);
  if (f != NULL)
    {
      list_remove (&f->elem);
      free (f);
    }
  lock_release (&frame_lock);
}

struct frame *
find_frame (const void *key)
{
  for (struct list_elem *cursor = list_begin (&frame_table); cursor != list_end (&frame_table); cursor = list_next (cursor))
    {
      struct frame *f = list_entry (cursor, struct frame, elem);
      if (f->page == key)
        return f;
    }

  return NULL;
}
