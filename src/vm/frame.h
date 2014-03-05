#ifndef FRAME_H
#define FRAME_H

#include <list.h>
#include <stdint.h>

#include "threads/pte.h"
#include "threads/thread.h"
#include "vm/page.h"

/*! Store the frame table as a list, sorted by frame number. */
struct list frame_table;

/*! The frame queue -- used for implementing a second chance policy. */
struct list frame_queue;

/*! Frame struct used by the frame table to keep track of which frames are
    free and which frames are allocated. */
struct frame {
    /* The kernel virtual address of the frame. */
    void *kpage;
    /* The virtual address of the page that currently occupies the frame at 
       index FRAME_NUM. */
    void *upage;
    /* Store the pointer to the thread/process that owns this upage. */
    struct thread *thread;
    
    /* The list_elem in the frame table list. */
    struct list_elem elem;

    /* The list_elem for the frame queue for eviction policy. */
    struct list_elem q_elem;
};

void *frame_evict(void);
void frame_table_remove(struct frame *frame);
void frame_add(struct thread *t, void *upage, void *kpage);
bool frame_less(const struct list_elem *elem1, const struct list_elem *elem2, 
                void *aux UNUSED);

#endif
