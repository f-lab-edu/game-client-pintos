#include "page.h"
#include "frame.h"
#include "swap.h"
#include "debug.h"
#include "string.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static hash_hash_func page_hash;
static hash_less_func page_less;
static hash_action_func page_delete;

void page_table_init (struct hash *page_table)
{
  hash_init (page_table, page_hash, page_less, NULL);
}

void page_table_destroy (struct hash *page_table)
{
  hash_destroy (page_table, page_delete);
}

static unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, elem);
  return hash_bytes (&p->address, sizeof p->address);
}

static bool
page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  const struct page *pa = hash_entry (a, struct page, elem);
  const struct page *pb = hash_entry (b, struct page, elem);
  return pa->address < pb->address;
}

static void
page_delete (struct hash_elem *e, void *aux UNUSED)
{
  struct thread *t = thread_current ();
  struct page *p = hash_entry (e, struct page, elem);

  if (swap_table_is_swapped (t->tid, p->address))
    swap_table_destroy (t->tid, p->address);
  else
    {
      if (p->segment == SEG_MAPPING)
        {
          if (pagedir_is_dirty (t->pagedir, p->address))
            {
              void *kpage = frame_table_get_frame (p->address);
              file_seek (p->file, p->position);
              file_write (p->file, kpage, PGSIZE);
            }
        }
      frame_table_clear_frame (p->address);
    }

  free (p);
}

void
page_table_insert (struct hash *page_table, struct page *page)
{
  hash_insert (page_table, &page->elem);
}

struct page *
page_table_lookup (struct hash *page_table, const void *address)
{
  struct page p;
  struct hash_elem *e;

  p.address = address;
  e = hash_find (page_table, &p.elem);
  return e != NULL ? hash_entry (e, struct page, elem) : NULL;
}
