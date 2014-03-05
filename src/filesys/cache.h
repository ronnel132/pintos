#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <hash.h>
#include <list.h>
#include <stdlib.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

#define CACHE_SIZE 64


/* Read write lock */
struct rwlock {
    /* Lock to represent write lock */
    struct lock wl;

    /* Writer condition variable */
    struct condition w_cond;

    /* Mutex for small operations, like checking num_readers */
    struct lock mutex;

    /* Number of concurrent readers */
    int num_readers;
};



// TODO: DUMMY, remove
struct cache_desc {
    struct rwlock rwl;
};


/*! The mapping between filesys sector index and cache index. */
struct hash cache_table; 

/*! A queue of the element in the cache. Used for evicting */
struct list cache_queue;

/*! An entry in the filesystem buffer cache. */
struct cache_entry {
    /* The sector in the filesys block the cached block corresponds to. */
    block_sector_t sector_idx;

    void *data;
    
    /* Has this cached block been accessed? */
    bool accessed;

    /* Has this cached block been written to? */
    bool dirty;

    /*! The element stored in the hash table for the buffer cache. */
    struct hash_elem elem;

    /*! The linked list queue pointer. */
    struct list_elem q_elem;
}; 

void cache_init(void); 
void cache_read(block_sector_t sector_idx, void *buffer, off_t size, 
                off_t offset);
void cache_write(block_sector_t sector_idx, void *buffer, off_t size,
                 off_t offset);
unsigned cache_hash(const struct hash_elem *element, void *aux);
bool cache_less(const struct hash_elem *a, const struct hash_elem *b, 
                void *aux);



/* Acquire a read lock for this cache descriptor */
static void read_lock(struct cache_desc *cd);
/* Release a read lock for this cache descriptor */
static void read_unlock(struct cache_desc *cd);

/* Acquire a write lock for this cache descriptor */
static void write_lock(struct cache_desc *cd);
/* Release a write lock for this cache descriptor */
static void write_unlock(struct cache_desc *cd);

#endif /* filesys/cache.h */
