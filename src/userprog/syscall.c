#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "devices/shutdown.h"

#include "threads/vaddr.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/file.h"

#include "devices/input.h"

#include "userprog/process.h"

/* Frees a page */
extern void palloc_free_page (void *);

/* Validates a user-provided pointer. Checks that it's in the required
 * range, and that it correpsonds to a page directory entry
 */
bool valid_user_pointer(const void *ptr, const void *esp);

/* Checks if a file is open */
bool file_is_open(int fd);

/* Gets the file struct from a file descriptor */
struct file * get_file_struct(int fd);

/* Function prototype */
static void syscall_handler(struct intr_frame *);

/* Global thread lists */
extern struct list dead_list;
extern struct list all_list;


/* Validates a user-provided pointer. Checks that it's in the required
 * range, and that it correpsonds to a page directory entry
 */
bool valid_user_pointer(const void *ptr, const void *esp) {
    uint32_t *pd, *pde;


    pd = active_pd();
    pde = pd + pd_no(ptr);

    /* See lookup_page() for more info */
    if (is_user_vaddr(ptr) && (*pde != 0)) {
        if ((esp != NULL) && (ptr < esp)) {
            return false;
        }
        return true;
    }
//     printf("%d\n", is_user_vaddr(ptr));
//     printf("%d\n", *pde);
//     printf("%p\n", esp);
//     printf("%d\b", (*(void **) ptr < esp));
//     printf("===========\n");

    return false;
}


/* Helper functions for files related to process. */

/* Checks if a file is open */
bool file_is_open(int fd) {
    bool opn = false;

    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        opn = thread_current()->process_details->open_file_descriptors[fd];
    }

    return opn;
}

/* Gets the file struct from a file descriptor */
struct file * get_file_struct(int fd) {
    struct file *f = NULL;

    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        f = thread_current()->process_details->files[fd];
    }

    return f;
}


/* Installs the syscall_handler into the interrupt vector table. */
void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Based on the stack when interrupted, syscall_handler executes correct
 * syscall according to the syscall number and with
 * the proper arguments.
 */
static void syscall_handler(struct intr_frame *f UNUSED) {
    /* Get esp address */
    void *esp = (void *) f->esp;

    /* Store esp in thread struct */
    thread_current()->esp = esp;

//     printf("syscall!\n");

    /* Check validity of syscall_nr */
    if (!valid_user_pointer(esp, NULL)) {
        exit(EXIT_BAD_PTR);
    }

    /* Extract syscall numbers and arguments */
    int syscall_nr = *((int *)f->esp);
//     printf("%d\n", syscall_nr);



    void *arg1 = (void *) ((int *)(f->esp + 4));
    void *arg2 = (void *) ((int *)(f->esp + 8));
    void *arg3 = (void *) ((int *)(f->esp + 12));
    void *arg4 = (void *) ((int *)(f->esp + 16));


    /* Figure out which system call was invoked. Also note that we check the
     * passed pointers for correctness. There probably is a more elegant way
     * to write this; sorry about that
     */
    switch(syscall_nr) {
        case SYS_HALT:
            halt();
            break;

        case SYS_EXIT:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            exit(*((int *)arg1));
            break;

        case SYS_EXEC:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = exec(*((const char **)arg1));
            break;

        case SYS_WAIT:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = wait(*((pid_t *)arg1));
            break;

        case SYS_CREATE:
            if ((!valid_user_pointer(arg1, NULL)) || (!valid_user_pointer(arg2, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = create(*((const char **)arg1), *((unsigned *)arg2)); 
            break;

        case SYS_REMOVE:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = remove(*((const char **)arg1));
            break; 

        case SYS_OPEN:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = open(*((const char **)arg1));
            break;

        case SYS_FILESIZE:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = filesize(*((int *) arg1));
            break;

        case SYS_READ:
            if ((!valid_user_pointer(arg1, NULL)) ||
                (!valid_user_pointer(arg2, NULL)) ||
                (!valid_user_pointer(arg3, NULL))) {
//                     printf("syscall check failed\n");

                exit(EXIT_BAD_PTR);
            }
            f->eax = read(*((int *) arg1),
                          *((void **) arg2),
                          *((unsigned *) arg3));
            break;

        case SYS_WRITE:
//             if ((!valid_user_pointer(arg1, NULL))) {
//                 exit(EXIT_BAD_PTR);
//             }
// 
//             if    (!valid_user_pointer(arg2, esp) && (* (int *) arg1 > 1)) {
//                 exit(EXIT_BAD_PTR);
//             }
// 
//             if    (!valid_user_pointer(arg3, NULL)) {
//                 exit(EXIT_BAD_PTR);
//             }

            if ((!valid_user_pointer(arg1, NULL)) ||
                (!valid_user_pointer(arg2, NULL)) ||
                (!valid_user_pointer(arg3, NULL))) {
                    printf("syscall check failed\n");

                exit(EXIT_BAD_PTR);
            }

            f->eax = write(*((int *) arg1),
                           *((void **) arg2),
                           *((unsigned *) arg3));
            break;

        case SYS_SEEK:
            if ((!valid_user_pointer(arg1, NULL)) || (!valid_user_pointer(arg2, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            seek(*((int *)arg1), *((unsigned *)arg2)); 
            break;

        case SYS_TELL:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = tell(*((int *) arg1));
            break;

        case SYS_CLOSE:
            if ((!valid_user_pointer(arg1, NULL))) {
                exit(EXIT_BAD_PTR);
            }
            close(*((int *) arg1));
            break;
        default:
            /* Yeah, we're not that nice */
            exit(-1);
    }
}


/* Terminates Pintos by calling shutdown_power_off() (declared in 
 * "devices/shutdown.h").  
 */
void halt (void) {
    shutdown_power_off();
}

/* Terminates the current user program, returning status to the kernel. */
void exit(int status) {
    thread_current()->exit_status = status;
    thread_exit();
}

/* Runs the executable whose name is given in cmd_line, passing any given 
 * arguments, and returns the new process's program id (pid).
 */
pid_t exec(const char *cmd_line) {
    if (!valid_user_pointer(cmd_line, NULL)) {
        exit(EXIT_BAD_PTR);
    }

    tid_t tid = -1;
    tid = process_execute(cmd_line);


    /* Wait for process to actually load */
    sema_down(thread_current()->child_loaded_sema);

    if (thread_current()->child_loaded_error == 1) {
        tid = -1;
    }
    return tid;
}

/* Waits for a child process pid and retrieves the child's exit status. */
int wait(pid_t pid) {
    tid_t tid = (tid_t) pid;
    enum intr_level old_level;
    int status = -1;
    struct list_elem *e;
    struct thread *iter;
    struct thread *waitee = NULL;
    struct semaphore *waitee_sema = NULL;
    struct thread_dead *dead;


    /* Disable interrupts while accessing global state. */
    old_level = intr_disable();
    /* If child is running, down its semaphore hence blocking yourself */
    for (e = list_begin(&all_list); e != list_end(&all_list); 
         e = list_next(e)) {
        iter = list_entry(e, struct thread, allelem);
        if (iter->tid == tid) {
            /* Check that it's our own child */
            if (iter->process_details->parent_id != thread_current()->tid) {
                return -1;
            }
            waitee = iter;
            waitee_sema = waitee->waiter_sema;
            break;
        } 
    }
    intr_set_level(old_level);

    if (waitee != NULL) {
        sema_down(waitee_sema);
    }

    old_level = intr_disable();
    /* If we're here, the child should be already dead */
    for (e = list_begin(&dead_list); e != list_end(&dead_list); 
         e = list_next(e)) {
        dead = list_entry(e, struct thread_dead, elem);
        if (dead->tid == tid) {
            /* Check that it's our own child */
            if (dead->parent_id != thread_current()->tid) {
                return -1;
            }
            status =  dead->status;
            list_remove(&dead->elem);

            /* Free thread_dead struct */
            palloc_free_page(dead);

            /* Free semaphor */
            palloc_free_page(waitee_sema);
            break;
        } 
    }
    intr_set_level(old_level);

    return status;
}

/* Creates a new file called file initially initial_size bytes in size. 
 * Returns true if successful, false otherwise. 
 */
bool create(const char *file, unsigned initial_size) {
    bool status = false;

    if (!valid_user_pointer(file, NULL)) {
        exit(EXIT_BAD_PTR);
    }

    /* Lock the file system while creating the file */
    lock_acquire(&filesys_lock);

    status = filesys_create(file, initial_size);

    /* Release the file system lock. */
    lock_release(&filesys_lock);

    return status;
}

/* Deletes the file called file. Returns true if successful, false otherwise.
 */
bool remove(const char *file) {
    bool status = false;

    if (!valid_user_pointer(file, NULL)) {
        exit(EXIT_BAD_PTR);
    }
    
    lock_acquire(&filesys_lock);
    status = filesys_remove(file);
    lock_release(&filesys_lock);

    return status;
}

/* Opens the file called file. Returns a nonnegative integer handle called 
 * a "file descriptor" (fd), or -1 if the file could not be opened. 
 */
int open(const char *file) {
    struct file * opened_file;
    struct process * pd;
    int file_descriptor = -1;
    unsigned i;

    if (!valid_user_pointer(file, NULL)) {
        exit(EXIT_BAD_PTR);
    }

    lock_acquire(&filesys_lock);

    /* Open the file and get the file struct from the filesystem. */
    opened_file = filesys_open(file);

    /* If file actually exists */
    if (opened_file != NULL) {
        /* Get pointer to the process_details */
        pd = thread_current()->process_details;

        /* If there are available file descriptors */
        if (pd->num_files_open < MAX_OPEN_FILES) {
            /* Search for first available file descriptor */
            for (i = 0; i < MAX_OPEN_FILES; i++) {
                /* The file descriptor is the index into the
                 * open_file_descriptors array.
                 */
                if (pd->open_file_descriptors[i] == false) {
                    file_descriptor = i;
                    break;
                }
            }

            /* Update the current process_details file details.
             * Set the file descriptor index to true, is open.
             * Set the file struct at the file descriptor index to the
             * one returned by the filesystem.
             * Increment the variable number of files that are open.
             */
            pd->open_file_descriptors[file_descriptor] = true;
            pd->files[file_descriptor] = opened_file;
            pd->num_files_open++;
        }
    }
    lock_release(&filesys_lock);

    return file_descriptor;
}


/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd) {
    int size = -1;

    lock_acquire(&filesys_lock);
    /* If file is indeed open, return length */
    if (file_is_open(fd)) {
        size = file_length(get_file_struct(fd));
    }
    lock_release(&filesys_lock);

    return size;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number
 * of bytes actually read (0 at end of file), or -1 if the file could not be
 * read (due to a condition other than end of file). Fd 0 reads from the
 * keyboard using input_getc().
 */
int read(int fd, void *buffer, unsigned size) {
    int bytes_read = -1;
    void *esp = thread_current()->esp;

    if (!valid_user_pointer(buffer, esp) || !valid_user_pointer(buffer + size, esp)) {
        exit(EXIT_BAD_PTR);
    }

//     printf("read_start\n");
    if (fd == STDIN_FILENO) {
        /* If file descriptor is the stdin descriptor, read from stdin to
         * the buffer.
         */
        while (size > 0) {
            /* Get uint8_t character from the console and set it where
             * the buffer points.
             */
            *((uint8_t *) buffer) = input_getc();

            /* Increment the buffer pointer */
            buffer = (void *) (((uint8_t *) buffer) + 1);

            /* Decrement the size for the while loop condition */
            size--;
        }
    } else if (fd != STDOUT_FILENO) {
        /* If the file descriptor is not stdout, read the file using
         * the filesystem.
         */
        lock_acquire(&filesys_lock);
        if (file_is_open(fd)) {
            bytes_read = file_read(get_file_struct(fd), buffer, size);
        }
        lock_release(&filesys_lock);
    }

    return bytes_read;
}

/* Writes size bytes from buffer to the open file fd. Returns the number of
 * bytes actually written, which may be less than size if some bytes could not
 * be written.
 */
int write(int fd, const void *buffer, unsigned size) {
    int bytes_written = -1;

    if (!valid_user_pointer(buffer, NULL) || !valid_user_pointer(buffer + size, NULL)) {
        exit(EXIT_BAD_PTR);
    }

    if (fd == STDOUT_FILENO) {
        /* If writing to stdout putbuf to output size bytes from buffer
         * to console.
         */
        putbuf((const char *) buffer, (size_t) size);
    } else if (fd != STDIN_FILENO) {
        /* If file is not stdin, write to file in the filesystem */
        lock_acquire(&filesys_lock);
        if (file_is_open(fd)) {
            bytes_written = file_write(get_file_struct(fd), buffer, size);
        }
        lock_release(&filesys_lock);
    }

    return bytes_written;
}

/* Changes the next byte to be read or written in open file fd to position,
 * expressed in bytes from the beginning of the file. (Thus, a position of 0
 * is the file's start.)
 */
void seek(int fd, unsigned position) {
    lock_acquire(&filesys_lock);
    if (file_is_open(fd)) {
        file_seek(get_file_struct(fd), position);
    }
    lock_release(&filesys_lock);
}

/* Returns the position of the next byte to be read or written in open file
 * fd, expressed in bytes from the beginning of the file.
 */
unsigned tell(int fd) {
    unsigned pos = 0;

    lock_acquire(&filesys_lock);
    if (file_is_open(fd)) {
        pos = file_tell(get_file_struct(fd));
    }
    lock_release(&filesys_lock);

    return pos;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly
 * closes all its open file descriptors, as if by calling this function for
 * each one.
 */
void close(int fd) {

    struct thread * cur_thread = thread_current();

    lock_acquire(&filesys_lock);
    if (file_is_open(fd)) {
        /* If file is opened by this process */

        /* Close the file using the filesystem. */
        file_close(get_file_struct(fd));

        /* Update the process_details struct.
         * Set the file pointer at the file descriptor index in the files
         * array to NULL.
         * Specify that the filedescriptor is now available.
         */
        thread_current()->process_details->files[fd] = NULL;
        thread_current()->process_details->open_file_descriptors[fd] = false;

        /* Decrement the variable of number of files that are open */
        cur_thread->process_details->num_files_open--;
    }
    lock_release(&filesys_lock);
}


/* Maps the file open as fd into the process's virtual address space. The
 * entire file is mapped into consecutive virtual pages starting at addr.
 */
mapid_t mmap(int fd, void *addr) {
    int size, num_pages;

//     if (!valid_user_pointer(addr, NULL)) {
//         exit(EXIT_BAD_PTR);
//     }

    /* TODO
    size =  = filesize(fd);
    num_pages = size / PGSIZE;

    if (size == 0) {
        return NULL;
    }
    if (pg_ofs(addr) != 0) {
        exit(EXIT_BAD_PTR);
    }


    if (size % PGSIZE != 0) {
        num_pages++;
        void *palloc_get_multiple (enum palloc_flags, size_t page_cnt);
        memset();
    } else {

    }
    */

    return NULL;
}

/* Unmaps the mapping designated by mapping, which must be a mapping ID
 * returned by a previous call to mmap by the same process that has not yet
 * been unmapped.
 */
void munmap(mapid_t mapping) {
    return;
}
