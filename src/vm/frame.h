#ifndef VM_FRAME_H
#define VM_FRAME_H

void frame_table_init (void);
void * frame_table_get_frame ();
void frame_table_free_frame (void *);


#endif /* vm/frame.h */