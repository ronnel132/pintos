#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

#include "filesys/filesys.h"
#include "filesys/inode.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    // TODO: Register all system calls
}

static void syscall_handler(struct intr_frame *f UNUSED) {
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
    return;
}

/* Runs the executable whose name is given in cmd_line, passing any given 
 * arguments, and returns the new process's program id (pid).
 */
pid_t exec(const char *cmd_line);

/* Waits for a child process pid and retrieves the child's exit status. */
int wait(pid_t pid) {
    return;
}

/* Creates a new file called file initially initial_size bytes in size. 
 * Returns true if successful, false otherwise. 
 */
bool create(const char *file, unsigned initial_size) {
    bool status = false;
    // TODO LOCK
    status = filesys_create(file, initial_size);
    // TODO UNLOCK
    return status;
}

/* Deletes the file called file. Returns true if successful, false otherwise.
 */
bool remove(const char (file) {
    bool status = false;
    // TODO LOCK
    status = filesys_remove(file, initial_size);
    // TODO UNLOCK
    return status;
}

/* Opens the file called file. Returns a nonnegative integer handle called 
 * a "file descriptor" (fd), or -1 if the file could not be opened. 
 */
int open(const char *file) {
    struct file * opened_file;
    struct process_details * pd;
    int file_descriptor = -1;
    unsigned i;

    // TODO filesys LOCK

    opened_file = filesys_open(file, initial_size);

    if (opened_file != NULL) {
        pd = thread_current()->process_details;

        if (pd->num_files_open < MAX_OPEN_FILES) {
            /* Search for first available file descriptor */
            for (i = 0; i < MAX_OPEN_FILES; i++) {
                if (cur_thread->process_details->open_file_descriptors[i] == false) {
                    file_descriptor = i;
                    break;
                }
            }

            pd->open_file_descriptors[file_descriptor] = true;
            pd->files[num_files_open] = opened_file;
            pds->num_files_open++;
        }
    }

    // TODO UNLOCK
    return file_descriptor;
}


/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd) {
    int size = -1;

    // TODO: LOCK NEEDED OR NOT??
    /* If file is indeed open, return length */
    if (thread_current()->process_details->open_file_descriptors[fd]) {
        size = inode_length(file_get_inode(pd->files[fd]));
    }
    // TODO UNLOCK

    return file_descriptor;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number
 * of bytes actually read (0 at end of file), or -1 if the file could not be
 * read (due to a condition other than end of file). Fd 0 reads from the
 * keyboard using input_getc().
 */
int read(int fd, void *buffer, unsigned size) {
    struct file * f;
    int bytes_read = -1;

    // TODO LOCKS
    if (file_is_open(fd)) {
        bytes_read = file_read(get_file_struct(fd), buffer, size);
    }
    // TODO UNLOCK

    return bytes_read;
}

/* Writes size bytes from buffer to the open file fd. Returns the number of
 * bytes actually written, which may be less than size if some bytes could not
 * be written.
 */
int write(int fd, const void *buffer, unsigned size) {
    struct file * f;
    int bytes_written = -1;

    // TODO LOCKS
    if (file_is_open(fd)) {
        bytes_written = file_write(get_file_struct(fd), buffer, size);
    }
    // TODO UNLOCK

    return bytes_written;
}

/* Changes the next byte to be read or written in open file fd to position,
 * expressed in bytes from the beginning of the file. (Thus, a position of 0
 * is the file's start.)
 */
void seek(int fd, unsigned position) {
    return false;
}

/* Returns the position of the next byte to be read or written in open file
 * fd, expressed in bytes from the beginning of the file.
 */
unsigned tell(int fd) {
    return 0;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly
 * closes all its open file descriptors, as if by calling this function for
 * each one.
 */
 void close(int fd) {
    return;
 }

/* Returns true if file descriptor refers to an open file */
bool file_is_open(int fd) {
    return thread_current()->process_details->open_file_descriptors[fd];
}

struct file * get_file_struct(int fd) {
    return thread_current()->process_details->files[fd];
}
