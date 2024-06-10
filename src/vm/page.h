#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "hash.h"
#include "filesys/off_t.h"

enum segment
  {
    SEG_CODE,
    SEG_STACK,
  };

struct page
  {
    struct hash_elem elem;
    void *address;
    enum segment segment;
    bool writable;
    off_t position;
  };

void page_table_init (struct hash *page_table);
void page_table_destroy (struct hash *page_table);
void page_table_insert (struct hash * page_table, struct page *page);
struct page * page_table_lookup (struct hash *page_table, const void *address);

#endif /* vm/page.h */