#ifndef VM_FRAME_H
#define VM_FRAME_H

void frame_table_init (void);
void * frame_table_set_frame (void *upage);
void * frame_table_get_frame (void *upage);
void frame_table_clear_frame (void *upage);
void frame_table_lock_frame (void *kpage);
void frame_table_unlock_frame (void *kpage);

#endif /* vm/frame.h */