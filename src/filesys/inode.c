#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
/*! Maximum number of sectors indices that can be stored on a single index. */
#define IDX_PER_SECTOR 128
/*! Number of blocks used for on-disk inode index structure. */
#define IDX_BLOCKS 126
/*! Number of direct blocks in the index structure. */
#define DIRECT_BLOCKS (IDX_BLOCKS - 2)
/*! Index of the indirect block in the on-disk inode blocks array. */
#define INDIRECT_IDX (IDX_BLOCKS - 2)
/*! Index of the doubly indirect block in the on-disk inode blocks array. */
#define DBL_INDIRECT_IDX (INDIRECT_IDX + 1)
/*! The byte after the last byte addressable by the direct blocks. */
#define DIRECT_UPPER (DIRECT_BLOCKS * BLOCK_SECTOR_SIZE)
/*! First byte addressable by the indirect block. */
#define INDIRECT_LOWER DIRECT_UPPER
/*! The byte after the last byte addressable by the indirect block. */
#define INDIRECT_UPPER (INDIRECT_LOWER + (IDX_PER_SECTOR * BLOCK_SECTOR_SIZE))
/*! Number of bytes addressable by a single index within the 1st level of
    the doubly indexed index blocks. */
#define BYTES_PER_DBL_IDX (IDX_PER_SECTOR * BLOCK_SECTOR_SIZE)
/*! First byte addressable by the doubly indirect block. */
#define DBL_INDIRECT_LOWER INDIRECT_UPPER
/*! The byte after the last byte addressable by the doubly indirect block. */
#define DBL_INDIRECT_UPPER (DBL_INDIRECT_LOWER + \
                           (IDX_PER_SECTOR * IDX_PER_SECTOR * \
                            BLOCK_SECTOR_SIZE))

/*! On-disk inode.
    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    off_t length;                       /*!< File size in bytes. */
    unsigned magic;                     /*!< Magic number. */
    int blocks[IDX_BLOCKS]; /*!< Not used. */
};

/*! Returns the number of sectors to allocate for an inode SIZE
    bytes long. */
static inline size_t bytes_to_sectors(off_t size) {
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/*! In-memory inode. */
struct inode {
    struct list_elem elem;              /*!< Element in inode list. */
    block_sector_t sector;              /*!< Sector number of disk location. */
    int open_cnt;                       /*!< Number of openers. */
    bool removed;                       /*!< True if deleted, false otherwise. */
    int deny_write_cnt;                 /*!< 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /*!< Inode content. */
};

/*! Allocates a new sector, and returns its sector index in the file system.
    If INDEX_BLOCK is true, write -1 to all indices of the sector. 
    Returns -1 if allocation is unsuccessful. */
static block_sector_t allocate_sector(bool index_block) {
    int i;
    block_sector_t sector; 
    if (free_map_allocate(1, &sector)) {
        int sector_empty[IDX_PER_SECTOR];
        if (index_block) {
            /* Fill the sector_empty buffer with -1's, representing a file
               index block without any data. */
            for (i = 0; i < IDX_PER_SECTOR; i++) {
                sector_empty[i] = -1;   
            } 
        }
        else {
            for (i = 0; i < IDX_PER_SECTOR; i++) {
                sector_empty[i] = 0;
            }
        }
        cache_write(sector, sector_empty, BLOCK_SECTOR_SIZE, 0);
        return sector;
    } 
    return -1;
}

/*! Allocates all necessary sectors to ensure that the file for DISK_INODE is
    addressable at offset POS. INODE_SECT is the sector where DISK_INODE is 
    stored. */
static bool allocate_at_byte(struct inode_disk *disk_inode, 
                             block_sector_t inode_sect, off_t pos) {
    int tmp_block[IDX_PER_SECTOR];
    int block_idx;
    block_sector_t sector, sector_idx;

    ASSERT(disk_inode != NULL);

    /* In the direct blocks. */
    if (pos < DIRECT_UPPER) {
        /* The byte is in the direct block region. */
        block_idx = pos / BLOCK_SECTOR_SIZE;
        
        if (disk_inode->blocks[block_idx] == -1) {
            sector = allocate_sector(false);
            if ((int) sector != -1) {
                /* Store this change on the in-memory inode. */
                disk_inode->blocks[block_idx] = sector;
                /* Store this change on the on-disk inode. */
                cache_write(inode_sect, disk_inode, BLOCK_SECTOR_SIZE, 0);
                return true;
            }
        }
    }
    /* In the singly indirect blocks. */
    else if (pos >= INDIRECT_LOWER && pos < INDIRECT_UPPER) {
        block_idx = (pos - INDIRECT_LOWER) / BLOCK_SECTOR_SIZE;

        if (disk_inode->blocks[INDIRECT_IDX] != -1) {
            /* Read the singly indirect block into the tmp block. */
            cache_read(disk_inode->blocks[INDIRECT_IDX], tmp_block, 
                       BLOCK_SECTOR_SIZE, 0);

            if (tmp_block[block_idx] == -1) {
                sector = allocate_sector(false);
                if ((int) sector != -1) {
                    tmp_block[block_idx] = sector;
                    cache_write(disk_inode->blocks[INDIRECT_IDX], tmp_block, 
                                BLOCK_SECTOR_SIZE, 0);
                    return true;
                }
            }
        }
        else {
            sector = allocate_sector(true);
            if ((int) sector != -1) {
                disk_inode->blocks[INDIRECT_IDX] = sector;
                cache_write(inode_sect, disk_inode, BLOCK_SECTOR_SIZE, 0);
                /* Call recursively after allocating the necessary index
                   block -- effectively, "try again" to get the sector. */
                return allocate_at_byte(disk_inode, inode_sect, pos);
            }
        }
    } 
    /* In the doubly indirect blocks. */
    else if (pos >= DBL_INDIRECT_LOWER && pos < DBL_INDIRECT_UPPER) {
        /* First level block index for doubly indexed block. */
        block_idx = (pos - DBL_INDIRECT_LOWER) / BYTES_PER_DBL_IDX; 

        if (disk_inode->blocks[DBL_INDIRECT_IDX] != -1) {
            /* Read the first doubly indirect block into the tmp block. */
            cache_read(disk_inode->blocks[DBL_INDIRECT_IDX], tmp_block, 
                       BLOCK_SECTOR_SIZE, 0);
            /* Check if the sector pointer in the block directory is valid. */
            if (tmp_block[block_idx] != -1) {
                sector_idx = tmp_block[block_idx];
                cache_read(sector_idx, tmp_block, BLOCK_SECTOR_SIZE, 0);
                /* Calculate the index of within the second doubly indirect 
                   block. */
                block_idx = ((pos - DBL_INDIRECT_LOWER) % BYTES_PER_DBL_IDX) /
                            BLOCK_SECTOR_SIZE;
                /* Check if the sector is valid. */
                if (tmp_block[block_idx] == -1) {
                    sector = allocate_sector(false);
                    tmp_block[block_idx] = sector;
                    cache_write(sector_idx, tmp_block, BLOCK_SECTOR_SIZE, 0);
                }
            }
            else {
                sector = allocate_sector(true);
                if (sector != 1) {
                    tmp_block[block_idx] = sector;
                    cache_write(disk_inode->blocks[DBL_INDIRECT_IDX],
                                tmp_block, BLOCK_SECTOR_SIZE, 0);
                    /* Retry. */
                    return allocate_at_byte(disk_inode, inode_sect, pos);
                }
            }
        }
        else {
            sector = allocate_sector(true);
            if ((int) sector != -1) {
                disk_inode->blocks[DBL_INDIRECT_IDX] = sector;
                cache_write(inode_sect, disk_inode, BLOCK_SECTOR_SIZE, 0);
                /* Retry. */
                return allocate_at_byte(disk_inode, inode_sect, pos);
            }
        }
    }
    return false;
}

/*! Returns the block device sector that contains byte offset POS
    within INODE.
    If one does not exist for offset POS, create one and return its sector if
    CREATE is TRUE.
    Otherwise return -1. */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos) {
    int tmp_block[IDX_PER_SECTOR];
    int block_idx;
    block_sector_t sector_idx;

    ASSERT(inode != NULL);

    /* In the direct blocks. */
    if (pos < DIRECT_UPPER) {
        /* The byte is in the direct block region. */
        block_idx = pos / BLOCK_SECTOR_SIZE;
        
        if (inode->data.blocks[block_idx] != -1) {
            return inode->data.blocks[block_idx];
        } 
    }
    /* In the singly indirect blocks. */
    else if (pos >= INDIRECT_LOWER && pos < INDIRECT_UPPER) {
        block_idx = (pos - INDIRECT_LOWER) / BLOCK_SECTOR_SIZE;
        if (inode->data.blocks[INDIRECT_IDX] != -1) {
            /* Read the singly indirect block into the tmp block. */
            cache_read(inode->data.blocks[INDIRECT_IDX], tmp_block, 
                       BLOCK_SECTOR_SIZE, 0);
            if (tmp_block[block_idx] != -1) {
                return tmp_block[block_idx];
            }
        }
    } 
    /* In the doubly indirect blocks. */
    else if (pos >= DBL_INDIRECT_LOWER && pos < DBL_INDIRECT_UPPER) {
        /* First level block index for doubly indexed block. */
        block_idx = (pos - DBL_INDIRECT_LOWER) / BYTES_PER_DBL_IDX; 

        if (inode->data.blocks[DBL_INDIRECT_IDX] != -1) {
            /* Read the first doubly indirect block into the tmp block. */
            cache_read(inode->data.blocks[DBL_INDIRECT_IDX], tmp_block, 
                       BLOCK_SECTOR_SIZE, 0);
            /* Check if the sector pointer in the block directory is valid. */
            if (tmp_block[block_idx] != -1) {
                sector_idx = tmp_block[block_idx];
                cache_read(sector_idx, tmp_block, BLOCK_SECTOR_SIZE, 0);
                /* Calculate the index of within the second doubly indirect 
                   block. */
                block_idx = ((pos - DBL_INDIRECT_LOWER) % BYTES_PER_DBL_IDX) /
                            BLOCK_SECTOR_SIZE;
                /* Check if the sector is valid. */
                if (tmp_block[block_idx] != -1) {
                    return tmp_block[block_idx];
                }
            }
        }
    }
    return -1;
}

/*! List of open inodes, so that opening a single inode twice
    returns the same `struct inode'. */
static struct list open_inodes;

/*! Initializes the inode module. */
void inode_init(void) {
    list_init(&open_inodes);
}

/*! Initializes an inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length) {
    struct inode_disk *disk_inode = NULL;
    int i;

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    
    /* Initialize the disk_inode blocks array to all be -1. */
    for (i = 0; i < (DIRECT_BLOCKS + 2); i++) {
        disk_inode->blocks[i] = -1;
    }

    if (disk_inode != NULL) {
        size_t sectors = bytes_to_sectors(length);
        disk_inode->length = length;
        disk_inode->magic = INODE_MAGIC;
        for (i = 0; i < (int) sectors; i += BLOCK_SECTOR_SIZE) {
            if (!allocate_at_byte(disk_inode, sector, i)) {
                free(disk_inode);
                goto fail;
            }
        }
        cache_write(sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
        return true; 
    }
fail:
    return false;
}

/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            return inode; 
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    cache_read(inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
    return inode;
}

/*! Reopens and returns INODE. */
struct inode * inode_reopen(struct inode *inode) {
    if (inode != NULL)
        inode->open_cnt++;
    return inode;
}

/*! Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode *inode) {
    return inode->sector;
}

/*! Closes INODE and writes it to disk.
    If this was the last reference to INODE, frees its memory.
    If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
    off_t length, i;
    /* Ignore null pointer. */
    if (inode == NULL)
        return;

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);
 
        /* Deallocate blocks if removed. */
        if (inode->removed) {
            free_map_release(inode->sector, 1);
            length = inode->data.length;
            for (i = 0; i < length; i += BLOCK_SECTOR_SIZE) {
                free_map_release(byte_to_sector(inode, i), 1);
            }
        }

        free(inode); 
    }
}

/*! Marks INODE to be deleted when it is closed by the last caller who
    has it open. */
void inode_remove(struct inode *inode) {
    ASSERT(inode != NULL);
    inode->removed = true;
}

/*! Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset) {
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    
    while (size > 0) {
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        if ((int) sector_idx == -1) {
            if (offset < inode_length(inode)) {
                allocate_at_byte(&inode->data, inode->sector, offset);
                sector_idx = byte_to_sector(inode, offset); 
                ASSERT((int) sector_idx != -1);
            }
            else {
                return bytes_read;
            }
        }
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0) {
            break;
        }

        cache_read(sector_idx, buffer + bytes_read, chunk_size, sector_ofs);  


        if ((((offset + BLOCK_SECTOR_SIZE) >> 9) << 9) < inode_length(inode)) {
            block_sector_t next_sector_idx = byte_to_sector(inode, 
                                (((offset + BLOCK_SECTOR_SIZE) >> 9) << 9));
            if (next_sector_idx == -1) {
                allocate_at_byte(&inode->data, inode->sector, 
                                 (((offset + BLOCK_SECTOR_SIZE) >> 9) << 9));
                next_sector_idx = byte_to_sector(inode, 
                                (((offset + BLOCK_SECTOR_SIZE) >> 9) << 9));  
            }
            read_ahead(sector_idx, next_sector_idx);
        }

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }

    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if end of file is reached or an error occurs.
    (Normally a write at end of file would extend the inode, but
    growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size, off_t offset) {
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    off_t original_size = size;
    bool tmp;

    if (inode->deny_write_cnt)
        return 0;
    
    /* Extend the file if needed. */
    if (offset + size > inode_length(inode)) {
        inode->data.length = offset + size;
        cache_write(inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
    }

    while (size > 0) {
        /* Sector to write, starting byte offset within sector. */
        /* TODO: sector_idx = merge conflict third parameter "true"? */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        /* If there is no sector assigned to this offset, assign one. */
        while ((int) sector_idx == -1) {
            tmp = allocate_at_byte(&inode->data, inode->sector, offset);
            /* Try again. */
            sector_idx = byte_to_sector(inode, offset);
        }
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0) {
            break;
        }

        cache_write(sector_idx, buffer + bytes_written, chunk_size, sector_ofs);

        if ((((offset + BLOCK_SECTOR_SIZE) >> 9) << 9) < inode_length(inode)) {
            block_sector_t next_sector_idx = byte_to_sector(inode, 
                                (((offset + BLOCK_SECTOR_SIZE) >> 9) << 9));
            if (next_sector_idx == -1) {
                allocate_at_byte(&inode->data, inode->sector, 
                                 (((offset + BLOCK_SECTOR_SIZE) >> 9) << 9));
                next_sector_idx = byte_to_sector(inode, 
                                (((offset + BLOCK_SECTOR_SIZE) >> 9) << 9));  
            }
            read_ahead(sector_idx, next_sector_idx);
        }

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }

    return bytes_written;
}

/*! Disables writes to INODE.
    May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) {
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/*! Re-enables writes to INODE.
    Must be called once by each inode opener who has called
    inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode) {
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/*! Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode) {
    return inode->data.length;
}

