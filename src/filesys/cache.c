#include <string.h>
#include <stdio.h>
#include <list.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"

/* Max size of read ahead list */
#define MAX_RA_LIST 32

/* Acquire a read lock for this cache descriptor */
static void read_lock(struct cache_block *cb);
/* Release a read lock for this cache descriptor */
static void read_unlock(struct cache_block *cb);

/* Acquire a write lock for this cache descriptor */
static void write_lock(struct cache_block *cb);
/* Release a write lock for this cache descriptor */
static void write_unlock(struct cache_block *cb);


/* Read an sector through the cache. Will trigger a cache_miss
 * if the sector isn't cached */
static void _cache_read(block_sector_t sector_idx, void *buffer, off_t size, 
                off_t offset, int *error);

/* Write an sector through the cache. Will trigger a cache_miss
 * if the sector isn't cached */
static void _cache_write(block_sector_t sector_idx, const void *buffer, off_t size,
                off_t offset, int *error);

/* Called in cache_read and cache_write when the desired block is not cached.
   Store it in the cache, then return its corresponding cache_entry struct. */
static struct cache_entry *cache_miss(block_sector_t sector_idx);

/*! Gets cache table entry from the hash table. Returns NULL if
 *  sector is not in present in the hash table.
 */
static struct cache_entry * cache_get_entry(block_sector_t sector_idx);

/* Write-back daemon */
static void write_behind_daemon(void * aux);

/* Flags to control when to stop the write behind and
 * read ahead daemons
 */
bool rad_stop = false;
bool wbd_stop = false;

/* Read ahead list. */
static void read_ahead_daemon(void * aux);
struct condition ra_cond;
static struct list read_ahead_list;

/* Read ahead list lock */
struct lock ra_lock;

/* Hashtable lock */
struct lock ht_lock;

/*! The mapping between filesys sector index and cache index. */
struct hash cache_table; 

/*! The actual cache itself. An array of cache_block's. */
struct cache_block cache[CACHE_SIZE];

/* Lock to protect hand pointer */
struct lock hand_lock;

/*! The clock hand index for implementing the clock policy. Corresponds to an 
    index in cache. */ 
static int hand;

/*! Initialize the buffer cache by malloc'ing the space it needs. */
void cache_init(void) {
    int i;

    for (i = 0; i < CACHE_SIZE; i++) {
        cache[i].sector_idx = 0;
        cache[i].valid = 0;
        cache[i].accessed = 0;
        cache[i].dirty = 0;
        
        /* Init rwlock stuff */
        lock_init(&(cache[i].rwl.wl));
        lock_init(&(cache[i].rwl.mutex));
        cache[i].rwl.num_readers = 0;
        cond_init(&(cache[i].rwl.w_cond));
        cond_init(&(cache[i].rwl.r_cond));
        cond_init(&ra_cond);
    }

    hash_init(&cache_table, &cache_hash, &cache_less, NULL);
    hand = 0;

    /* Initialize hand lock */
    lock_init(&hand_lock);

    /* Initialize ra lock */
    lock_init(&ra_lock);

    /* Initialize ht lock */
    lock_init(&ht_lock);

    /* Initialize read_ahead and write_behind threads. */
    cond_init(&ra_cond);
    list_init(&read_ahead_list);


    /* Create read ahead and write behind daemons */
    thread_create("rad",
                  PRI_MIN + 1,
                  read_ahead_daemon,
                  NULL);
    thread_create("wbd", PRI_MIN + 1, write_behind_daemon, NULL);
}

/* Find and evict an entry from the cache. A clock
 * policy is being used */
void cache_evict() {
    struct cache_block *cb;
    struct cache_entry ce;
    struct cache_entry *tmp;
    struct hash_elem *elem;
    bool locked = false;
    ASSERT(lock_held_by_current_thread(&hand_lock));

    /* Pick a cache entry to evict, and kick it out of the cache 
     * Using second chance strategy
     */
    while (1) {
        cb = &cache[hand];

        /* Lock this cache block */
        write_lock(cb);

        ASSERT(cb->valid);

        /* If it's been accessed, clear the accessed bit */
        if (cb->accessed) {
            cb->accessed = 0;

            /* Advance hand */
            hand = (hand + 1) % CACHE_SIZE;
            write_unlock(cb);
        }

        /* Else evict entry, unless it's pinned */
        else {

            if (!lock_held_by_current_thread(&ht_lock)) {
                lock_acquire(&ht_lock);
                locked = true;
            }
            
            tmp = cache_get_entry(cb->sector_idx); 

            /* Ignore pinned entries */
            if (tmp->pinned) {
                write_unlock(cb);
                hand = (hand + 1) % CACHE_SIZE;
                if (locked) {
                    lock_release(&ht_lock);
                }
                continue;
            }

            ce.sector_idx = cb->sector_idx;
            elem = hash_find(&cache_table, &ce.elem);
            /* The cache_entry corresponding to the sector in this cache index
               should be present in the cache_table. */
//             ASSERT(elem != NULL);
            if (elem == NULL) {
                ASSERT(0);
            }

            cb->valid = 0;
            hash_delete(&cache_table, elem);

            if (locked) {
                lock_release(&ht_lock);
            }

            /* If dirty, write it back */
            if (cb->dirty) {
                block_write(fs_device, cb->sector_idx, cb->data);
                cb->dirty = 0;
            }

            locked = false;

            if (!lock_held_by_current_thread(&ht_lock)) {
                lock_acquire(&ht_lock);
                locked = true;
            }
            
            free(hash_entry(elem, struct cache_entry, elem));

            if (locked) {
                lock_release(&ht_lock);
            }

            write_unlock(cb);
            break;
        }
    }

    /* Make sure that the block hand is pointing to is invalid.
     * cache_miss() will overwrite this */
    ASSERT(!cache[hand].valid);
}

/*! Gets cache table entry from the hash table. Returns NULL if
 *  sector is not in present in the hash table.
 */
static struct cache_entry *cache_get_entry(block_sector_t sector_idx) {
    struct cache_entry ce; 
    struct hash_elem *e;
    struct cache_entry *tmp;
    bool locked = false;

    /* Only acquire hashtable lock if we don't already have it */
    if (!lock_held_by_current_thread(&ht_lock)) {
        lock_acquire(&ht_lock);
        locked = true;
    }
    ce.sector_idx = sector_idx;
    e = hash_find(&cache_table, &ce.elem);
    if (e == NULL) {
        if (locked) {
            lock_release(&ht_lock);
        }
        return NULL;
    }
    else {
        tmp = hash_entry(e, struct cache_entry, elem);
        if (locked) {
            lock_release(&ht_lock);
        }
        return tmp;
    }
}


/* Called in cache_read and cache_write when the desired block is not cached.
   Store it in the cache, then return its corresponding cache_entry struct. */
static struct cache_entry *cache_miss(block_sector_t sector_idx) {
    struct cache_entry *centry; 
    struct cache_block *cblock;
    bool locked = false;


    centry = malloc(sizeof(struct cache_entry));
    if (centry == NULL) {
        PANIC("Filesystem cache failure.");
    }

    lock_acquire(&hand_lock);
    /* Only acquire hashtable lock if we don't already have it */
    if (!lock_held_by_current_thread(&ht_lock)) {
        lock_acquire(&ht_lock);
        locked = true;
    }

    if (hash_size(&cache_table) == CACHE_SIZE) {
        if (locked) {
            lock_release(&ht_lock);
        }
        cache_evict();
    }

    /* Index of new cahce entry is old hand (now invalid) */
    centry->cache_idx = hand; 
    hand = (hand + 1) % CACHE_SIZE;
    
    centry->sector_idx = sector_idx;  
    centry->pinned = false;  
    
    locked = false;
    if (!lock_held_by_current_thread(&ht_lock)) {
        lock_acquire(&ht_lock);
        locked = true;
    }

    /* Insert entry pointer in the hash table */
    hash_insert(&cache_table, &centry->elem);

    if (locked) {
        lock_release(&ht_lock);
    }

    cblock = &cache[centry->cache_idx];
    write_lock(cblock);

    cblock->sector_idx = sector_idx;
    cblock->accessed = 0;
    cblock->dirty = 0;
    cblock->valid = 1;
    block_read(fs_device, sector_idx, cblock->data);
    lock_release(&hand_lock);
    write_unlock(cblock);
    return centry;
}

/* Wrapper around _cache_read to retry for errors */
void cache_read(block_sector_t sector_idx, void *buffer, off_t size,
                off_t offset) {
    int cache_error = 0;
    _cache_read(sector_idx, buffer, size, offset, &cache_error);

    /* Retry if error flag was set */
    while (cache_error) {
//         printf("===retrying\n");
        cache_error = 0;
        _cache_read(sector_idx, buffer, size, offset, &cache_error);
    }
}

/*! Find the cached sector at SECTOR_IDX and read SIZE bytes at offset
    OFFSET. */
static void _cache_read(block_sector_t sector_idx, void *buffer, off_t size,
                off_t offset, int *error) {
    struct cache_entry *stored_ce;
    block_sector_t stored_cache_idx;
    struct cache_block *cblock;

    ASSERT(offset + size <= BLOCK_SECTOR_SIZE);

    lock_acquire(&ht_lock);
    stored_ce = cache_get_entry(sector_idx);
    
    /* If entry in cache */ 
    if (stored_ce != NULL) {
        stored_ce->pinned = true;
        stored_cache_idx = stored_ce->cache_idx;
    }

    /* Otherwise get it by cache_miss()ing */
    else {
        stored_ce = cache_miss(sector_idx);
        stored_ce->pinned = true;
        stored_cache_idx = stored_ce->cache_idx;
    }


    cblock = &cache[stored_cache_idx];
    read_lock(cblock);
    

    /* If that cache block has changed, abort */
    if (cblock->sector_idx != sector_idx) {
        *error = 1; 
        read_unlock(cblock);
        stored_ce->pinned = false;
        lock_release(&ht_lock);
        return;
    }
    lock_release(&ht_lock);
    
    cblock->accessed = 1;

    memcpy(buffer, (void *) ((off_t) cblock->data + offset), 
           (size_t) size);
    stored_ce->pinned = false;
    read_unlock(cblock);
}


/* Wrapper around _cache_write to retry for errors */
void cache_write(block_sector_t sector_idx, const void *buffer, off_t size,
                off_t offset) {
    int cache_error = 0;
    _cache_write(sector_idx, buffer, size, offset, &cache_error);

    /* Retry if error flag was set */
    while (cache_error) {
//         printf("===retrying write\n");
        cache_error = 0;
        _cache_write(sector_idx, buffer, size, offset, &cache_error);
    }
}

/*! Find the cached sector at SECTOR_IDX and write SIZE bytes starting at 
    OFFSET from BUFFER to the cached sector. */
static void _cache_write(block_sector_t sector_idx, const void *buffer, off_t size,
                 off_t offset, int *error) {
    struct cache_entry *stored_ce;
    struct cache_block *cblock;
    bool present;
    block_sector_t stored_cache_idx;


    ASSERT(offset + size <= BLOCK_SECTOR_SIZE); 

    lock_acquire(&ht_lock);

    stored_ce = cache_get_entry(sector_idx);
    /* If entry in cache */
    if (stored_ce != NULL) {
        stored_ce->pinned = true;
        stored_cache_idx = stored_ce->cache_idx;
    }
    /* Otherwise get it by cache_miss()ing */
    else {
        stored_ce = cache_miss(sector_idx);
        stored_ce->pinned = true;
        stored_cache_idx = stored_ce->cache_idx;
    }
    
    cblock = &cache[stored_ce->cache_idx];
    write_lock(cblock);

    /* If that cache block has changed, abort */
    if (cache[stored_cache_idx].sector_idx != sector_idx) {
        *error = 1;
        write_unlock(cblock);
        stored_ce->pinned = false;
        lock_release(&ht_lock);
        return;
    }
    lock_release(&ht_lock);
    
    cblock->accessed = 1;

    memcpy((void *) ((off_t) cblock->data + offset), buffer, 
           (size_t) size);
    cblock->dirty = 1;
    stored_ce->pinned = false;
    write_unlock(cblock);
}


/* Acquire a read lock for this cache descriptor */
static void read_lock(struct cache_block *cb) {
    ASSERT(cb != NULL);
    struct rwlock *rwl = &(cb->rwl);
    struct lock *mutex = &(rwl->mutex); 
    struct lock *wl = &(rwl->wl);

    lock_acquire(mutex);
    /* If no one has write lock */
    if (wl->holder == NULL) {
        rwl->num_readers++;
        lock_release(mutex);
    }

    /* Else wait for writer to finish */
    else {
        cond_wait(&(rwl->r_cond), mutex);
        rwl->num_readers++;
        lock_release(mutex);
    }
}


/* Release a read lock for this cache descriptor */
static void read_unlock(struct cache_block *cb) {
    ASSERT(cb != NULL);
    struct rwlock *rwl = &(cb->rwl);
    struct lock *mutex = &(rwl->mutex); 

    lock_acquire(mutex);
    rwl->num_readers--;
    
    /* If 0 readers, wake up one writer */
    if (rwl->num_readers == 0) {
        cond_signal(&(rwl->w_cond), mutex);
    }
    lock_release(mutex);
}



/* Acquire a write lock for this cache descriptor */
static void write_lock(struct cache_block *cb) {
    ASSERT(cb != NULL);
    struct rwlock *rwl = &(cb->rwl);
    struct lock *mutex = &(rwl->mutex); 
    struct lock *wl = &(rwl->wl);

    /* Wait for all readers to finish */
    while (1) {
        lock_acquire(mutex);
        /* See if there are readers */
        if (rwl->num_readers > 0) {
            /* Wait for them to finish */
            cond_wait(&(rwl->w_cond), mutex);
            lock_release(mutex);
        }

        /* If we're here, we have the mutex and there are no readers */
        else {
            lock_acquire(wl);
            lock_release(mutex);
            return;
        }
    }
}


/* Release a write lock for this cache descriptor */
static void write_unlock(struct cache_block *cb) {
    ASSERT(cb != NULL);
    struct rwlock *rwl = &(cb->rwl);
    struct lock *mutex = &(rwl->mutex); 
    struct lock *wl = &(rwl->wl);
    
    lock_acquire(mutex);

    /* If there's any readers, let's signal them */
    if (rwl->num_readers > 0) {
        cond_signal(&(rwl->r_cond), mutex);
    }
    lock_release(wl);
    lock_release(mutex);
}


/* Hashes a cache entry. The hash is its sector_id */
unsigned cache_hash(const struct hash_elem *element, void *aux UNUSED) {
    struct cache_entry *ce = hash_entry(element, struct cache_entry, elem);
    return hash_int((int) ce->sector_idx);
}


/* Helper function used to compae two cache entries */
bool cache_less(const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED) {
    struct cache_entry *ce1 = hash_entry(a, struct cache_entry, elem);
    struct cache_entry *ce2 = hash_entry(b, struct cache_entry, elem);
    return ce1->sector_idx < ce2->sector_idx;
}



/* Read asynchronously the next sector. Returns immediately.
 * Readahead-s aren't guaranteed, and depend on system load
 * and current read ahead queue size */
void read_ahead(block_sector_t sector_idx) {
    struct read_ahead_entry *raentry;

    ASSERT(sector_idx != -1);

    lock_acquire(&ra_lock);
    if (list_size(&read_ahead_list) <= MAX_RA_LIST) {
        raentry = (struct read_ahead_entry *) malloc(sizeof(struct read_ahead_entry));

        if (raentry != NULL) {
            raentry->sector_idx = sector_idx;
            list_push_back(&read_ahead_list, &raentry->elem);
            cond_signal(&ra_cond, &ra_lock);
        }
    }
    lock_release(&ra_lock);
}

/* Daemon that runs read_ahead on the qeueue. Waits for a 
 * signal from the producer */
static void read_ahead_daemon(void * aux) {
    struct read_ahead_entry *raentry;
    struct cache_entry *centry;
    struct cache_entry *next_centry;

    while (true) {
        lock_acquire(&ra_lock);

        /* Wait to be signaled */
        cond_wait(&ra_cond, &ra_lock);

        /* Check if stopping condition for daemon is true */
        if (rad_stop = true) {
            /* Make sure we don't leak locks */
            lock_release(&ra_lock);
            exit(0);
        }

        if (!list_empty(&read_ahead_list)) {
            /* Pop first read_ahead entry */
            raentry = list_entry(list_pop_front(&read_ahead_list),
                                 struct read_ahead_entry,
                                 elem);
            ASSERT(raentry != NULL);

            /* Check to see if it's already in cache */
            centry = cache_get_entry(raentry->sector_idx);

            /* If we don't have this in cache, load it */
            if (centry == NULL) {
                cache_miss(raentry->sector_idx);
            }

            free(raentry);
        }
        lock_release(&ra_lock);
    }
}

/* Daemon that does a periodic writeback */
void write_behind_daemon(void *aux) {
    while (1) {
        timer_msleep(5000);

        /* Check if stopping condition for daemon is true */
        if (wbd_stop = true) {

            exit(0);
        }
        write_behind();
    }
}

/* Writes back all dirty blocks to the device.
 * Should call every x seconds from write_behind_daemon().
 */
void write_behind() {
    int i;

    for (i = 0; i < CACHE_SIZE; i++) {
        read_lock(&cache[i]);
        if (cache[i].valid && cache[i].dirty) {
            block_write(fs_device, cache[i].sector_idx, cache[i].data);
        }
        cache[i].dirty = 0;
        read_unlock(&cache[i]);
    }
}
