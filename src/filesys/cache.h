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

/*! An entry in the cache array. Contains the data and relevent metadata. */
struct cache_block {
    /* readwrite lock */
    struct rwlock rwl;

    block_sector_t sector_idx; 
    
    /* TRUE if the block currently corresponds to a sector. FALSE otherwise. */
    bool valid;

    /* Has this cached block been accessed? */
    bool accessed;

    /* Has this cached block been written to? */
    bool dirty;
    
    uint8_t data[BLOCK_SECTOR_SIZE];
};

/*! A mapping between sector index and cache index for quick accesses to 
    cached data (stored in a hash table). */
struct cache_entry {
    /* The sector in the filesys block the cached block corresponds to. */
    block_sector_t sector_idx;

    /* The index of this sector in the cache. */
    int cache_idx;

    /*! The element stored in the hash table for the buffer cache. */
    struct hash_elem elem;
}; 

void cache_init(void); 
void cache_read(block_sector_t sector_idx, void *buffer, off_t size, 
                off_t offset);
void cache_write(block_sector_t sector_idx, void *buffer, off_t size,
                 off_t offset);
unsigned cache_hash(const struct hash_elem *element, void *aux);
bool cache_less(const struct hash_elem *a, const struct hash_elem *b, 
                void *aux);

#endif /* filesys/cache.h */
