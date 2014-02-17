#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

#include "threads/vaddr.h"

#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/file.h"

#include "userprog/process.h"


#define file_is_open(fd) thread_current()->process_details->open_file_descriptors[fd]

#define get_file_struct(fd) thread_current()->process_details->files[fd]


static void syscall_handler(struct intr_frame *);

extern struct list dead_list;
extern struct list all_list;

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    // TODO: Register all system calls
}

static void syscall_handler(struct intr_frame *f UNUSED) {
    // TODO call appropriate function below
    printf("system call!\n");
    thread_exit();
}


/* Terminates Pintos by calling shutdown_power_off() (declared in 
 * "devices/shutdown.h").  
 */
void halt (void) {
    shutdown_power_off();
}

/* Terminates the current user program, returning status to the kernel. */
void exit(int status) {
    printf("in exit()\n");
    thread_current()->exit_status = status;
    process_exit();
}

/* Runs the executable whose name is given in cmd_line, passing any given 
 * arguments, and returns the new process's program id (pid).
 */
pid_t exec(const char *cmd_line) {
    tid_t tid = -1;
    // TODO: What synchronization are they suggesting in the assignment?
    // seems like this should work...
    if (is_user_vaddr(cmd_line)) {
        tid = process_execute(cmd_line);
    }
    return tid;
}

/* Waits for a child process pid and retrieves the child's exit status. */
int wait(pid_t pid) {
    tid_t tid = (tid_t) pid;
    int status = -2;
    struct list_elem *e;
    struct thread *iter;
    struct thread_dead *dead;
    printf("in wait()\n");

    while (1);

    /* TODO: Maybe child isn't created at this point! Use a semaphore or sth */

    /* TODO: Lock the ready_list somehow (disable interrupts?) */


    /* If child is running, down its semaphore hence blocking yourself */
    for (e = list_begin(&all_list); e != list_end(&all_list); 
         e = list_next(e)) {
        iter = list_entry(e, struct thread, elem);
        if (iter->tid == tid) {
            /* Check that it's our own child */
            if (iter->process_details->parent_id != thread_current()->tid) {
                return -1;
            }

            /* No one should have waited on this guy previously... */
            ASSERT(iter->waiter_sema->value == 1);

            sema_down(iter->waiter_sema);
            break;
        } 
    }

    /* If we're here, the child should be already dead */
    for (e = list_begin(&dead_list); e != list_end(&dead_list); 
         e = list_next(e)) {
        dead = list_entry(e, struct thread_dead, elem);
        if (dead->tid == tid) {
            /* Check that it's our own child */
            if (dead->parent_id != thread_current()->tid) {
                return -1;
            }
            return dead->status;
        } 
    }

    /* Shouldn't be here, so let's return -2 */
    return status;
}

/* Creates a new file called file initially initial_size bytes in size. 
 * Returns true if successful, false otherwise. 
 */
bool create(const char *file, unsigned initial_size) {
    bool status = false;

    if (is_user_vaddr(file)) {
        // TODO LOCK
        status = filesys_create(file, initial_size);
        // TODO UNLOCK
    }

    return status;
}

/* Deletes the file called file. Returns true if successful, false otherwise.
 */
bool remove(const char *file) {
    bool status = false;
    
    if (is_user_vaddr(file)) {
        // TODO LOCK
        status = filesys_remove(file);
        // TODO UNLOCK
    }

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

    printf("in open()\n");

    if (is_user_vaddr(file)) {
        // TODO filesys LOCK

        opened_file = filesys_open(file);

        if (opened_file != NULL) {
            pd = thread_current()->process_details;

            if (pd->num_files_open < MAX_OPEN_FILES) {
                /* Search for first available file descriptor */
                for (i = 0; i < MAX_OPEN_FILES; i++) {
                    if (pd->open_file_descriptors[i] == false) {
                        file_descriptor = i;
                        break;
                    }
                }

                pd->open_file_descriptors[file_descriptor] = true;
                pd->files[file_descriptor] = opened_file;
                pd->num_files_open++;
            }
        }

        // TODO UNLOCK
    }

    return file_descriptor;
}


/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd) {
    int size = -1;
    printf("in filesize()\n");
    struct process * pd = thread_current()->process_details;

    // TODO: LOCK NEEDED OR NOT??
    /* If file is indeed open, return length */
    if (pd->open_file_descriptors[fd]) {
        size = file_length(pd->files[fd]);
    }
    // TODO UNLOCK

    return size;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number
 * of bytes actually read (0 at end of file), or -1 if the file could not be
 * read (due to a condition other than end of file). Fd 0 reads from the
 * keyboard using input_getc().
 */
int read(int fd, void *buffer, unsigned size) {
    int bytes_read = -1;

    if (is_user_vaddr(buffer) && is_user_vaddr(buffer + size)) {
        // TODO LOCKS
        if (file_is_open(fd)) {
            bytes_read = file_read(get_file_struct(fd), buffer, size);
        }
        // TODO UNLOCK
    }

    return bytes_read;
}

/* Writes size bytes from buffer to the open file fd. Returns the number of
 * bytes actually written, which may be less than size if some bytes could not
 * be written.
 */
int write(int fd, const void *buffer, unsigned size) {
    int bytes_written = -1;

    if (is_user_vaddr(buffer) && is_user_vaddr(buffer + size)) {
        // TODO LOCKS
        if (file_is_open(fd)) {
            bytes_written = file_write(get_file_struct(fd), buffer, size);
        }
        // TODO UNLOCK
    }

    return bytes_written;
}

/* Changes the next byte to be read or written in open file fd to position,
 * expressed in bytes from the beginning of the file. (Thus, a position of 0
 * is the file's start.)
 */
void seek(int fd, unsigned position) {
    if (file_is_open(fd)) {
        file_seek(get_file_struct(fd), position);
    }
}

/* Returns the position of the next byte to be read or written in open file
 * fd, expressed in bytes from the beginning of the file.
 */
unsigned tell(int fd) {
    unsigned pos = 0;

    // TODO is this check needed??

    if (file_is_open(fd)) {
        pos = file_tell(get_file_struct(fd));
    }

    return pos;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly
 * closes all its open file descriptors, as if by calling this function for
 * each one.
 */
 void close(int fd) {

    printf("in close()\n");
    struct thread * cur_thread = thread_current();

     if (file_is_open(fd)) {
        file_close(get_file_struct(fd));
        thread_current()->process_details->files[fd] = NULL;

        cur_thread->process_details->num_files_open--;
    }
}
