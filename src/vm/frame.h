#ifndef FRAME_H
#define FRAME_H

#include <list.h>
#include <stdint.h>

#include "threads/thread.h"

/*! Store the frame table as a list, sorted by frame number. */
struct list frame_table;

/*! Frame struct used by the frame table to keep track of which frames are
    free and which frames are allocated. */
struct frame {
    /* The kernel virtual address of the frame. */
    void *kpage;
    /* The virtual address of the page that currently occupies the frame at 
       index FRAME_NUM. */
    void *upage;
    /* The pid of the process whose virtual address this is. */
    pid_t pid;
    struct list_elem elem;
};

void *frame_evict(void);

void frame_add(pid_t pid, void *upage, void *kpage);

bool frame_less(const struct list_elem *elem1, const struct list_elem *elem2, 
                void *aux UNUSED);

#endif
