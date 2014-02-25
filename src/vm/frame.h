#include <list.h>
#include <stdint.h>

#define FRAME_MAX 2 << 20

/*! Store the frame table as a list, sorted by frame number. */
list frame_table;

/*! Frame struct used by the frame table to keep track of which frames are
    free and which frames are allocated. */
struct frame {
    /* The frame number corresponding to the virtual address below. */ 
    int frame_num;
    /* The virtual address of the page that currently occupies the frame at 
       index FRAME_NUM. */
    void *vaddr;
    struct list_elem elem;
};

void *get_frame(enum palloc_flags);
void free_frame(void *);
