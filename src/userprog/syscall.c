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

bool valid_user_pointer(const void *ptr) {
    if (is_user_vaddr(ptr)) {
        return true;
    }

    return false;
}

bool file_is_open(int fd) {
    bool is_open = false;

    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        is_open = thread_current()->process_details->open_file_descriptors[fd];
    }

    return is_open;
}

int get_file_struct(int fd) {
    struct file *f = NULL;

    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        f = thread_current()->process_details->files[fd];
    }

    return f;
}

/*! Lock used by filesystem syscalls. */
extern struct lock filesys_lock;

static void syscall_handler(struct intr_frame *);


extern struct list dead_list;
extern struct list all_list;

/* Installs the syscall_handler into the interrupt vector table. */
void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED) {
    /* Check validity of syscall_nr */
    if (!valid_user_pointer(f->esp)) {
        exit(EXIT_BAD_PTR);
    }

    int syscall_nr = *((int *)f->esp);

    void *arg1 = (void *) ((int *)(f->esp + 4));
    void *arg2 = (void *) ((int *)(f->esp + 8));
    void *arg3 = (void *) ((int *)(f->esp + 12));
    void *arg4 = (void *) ((int *)(f->esp + 16));


//     printf("system call!\n");

    switch(syscall_nr) {
        case SYS_HALT:
            halt();
            break;

        case SYS_EXIT:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            exit(*((int *)arg1));
            break;

        case SYS_EXEC:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = exec(*((const char **)arg1));
            break;

        case SYS_WAIT:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = wait(*((pid_t *)arg1));
            break;

        case SYS_CREATE:
            if ((!valid_user_pointer(arg1)) || (!valid_user_pointer(arg2))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = create(*((const char **)arg1), *((unsigned *)arg2)); 
            break;

        case SYS_REMOVE:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = remove(*((const char **)arg1));
            break; 

        case SYS_OPEN:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = open(*((const char **)arg1));
            break;

        case SYS_FILESIZE:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = filesize(*((const char **)arg1));
            break;

        case SYS_READ:
            if ((!valid_user_pointer(arg1)) || (!valid_user_pointer(arg2)) ||
                  (!valid_user_pointer(arg3)) || (!valid_user_pointer(arg4))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = read(*((int *)arg1), *((void **)arg2), *((unsigned *)arg3));
            break;

        case SYS_WRITE:
            f->eax = write(*((int *)arg1), *((void **)arg2), *((unsigned *)arg3));
            break;

        case SYS_SEEK:
            if ((!valid_user_pointer(arg1)) || (!valid_user_pointer(arg2))) {
                exit(EXIT_BAD_PTR);
            }
            seek(*((int *)arg1), *((unsigned *)arg2)); 
            break;

        case SYS_TELL:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = tell(*((int *)arg1));
            break;

        case SYS_CLOSE:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            close(*((int *)arg1));
            break;
    }
//     printf("system call END \n");
}


/* Terminates Pintos by calling shutdown_power_off() (declared in 
 * "devices/shutdown.h").  
 */
void halt (void) {
    shutdown_power_off();
}

/* Terminates the current user program, returning status to the kernel. */
void exit(int status) {
//     printf("in exit()\n");
    thread_current()->exit_status = status;
//     process_exit();
    thread_exit();
}

/* Runs the executable whose name is given in cmd_line, passing any given 
 * arguments, and returns the new process's program id (pid).
 */
pid_t exec(const char *cmd_line) {
    tid_t tid = -1;
    // TODO: What synchronization are they suggesting in the assignment?
    // seems like this should work...
    if (valid_user_pointer(cmd_line)) {
        tid = process_execute(cmd_line);
    }
    // TODO: exiting?
    return tid;
}

/* Waits for a child process pid and retrieves the child's exit status. */
int wait(pid_t pid) {
    tid_t tid = (tid_t) pid;
    enum intr_level old_level;
    int status = -2;
    struct list_elem *e;
    struct thread *iter;
    struct thread *waitee = NULL;
    struct thread_dead *dead;
//     printf("in wait()\n");

//     for(;;);


    /* TODO: Maybe child isn't created at this point! Use a semaphore or sth */

    /* TODO: Lock the ready_list somehow (disable interrupts?) */


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
            status = iter->exit_status;
            waitee = iter;
            break;
        } 
    }
    intr_set_level(old_level);
    if (waitee != NULL) {
        sema_down(iter->waiter_sema);
        return status;
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

    if (valid_user_pointer(file)) {
        lock_acquire(&filesys_lock);
        status = filesys_create(file, initial_size);
        lock_release(&filesys_lock);
    } else {
        exit(EXIT_BAD_PTR);
    }

    return status;
}

/* Deletes the file called file. Returns true if successful, false otherwise.
 */
bool remove(const char *file) {
    bool status = false;
    
    if (valid_user_pointer(file)) {
        lock_acquire(&filesys_lock);
        status = filesys_remove(file);
        lock_release(&filesys_lock);
    } else {
        exit(EXIT_BAD_PTR);
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

//     printf("in open()\n");

    if (valid_user_pointer(file)) {
        lock_acquire(&filesys_lock);

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

        lock_release(&filesys_lock);
    } else {
        exit(EXIT_BAD_PTR);
    }

    return file_descriptor;
}


/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd) {
    int size = -1;
//     printf("in filesize()\n");

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

    if (valid_user_pointer(buffer) && valid_user_pointer(buffer + size)) {
        lock_acquire(&filesys_lock);
        if (file_is_open(fd)) {
            bytes_read = file_read(get_file_struct(fd), buffer, size);
        }
        lock_release(&filesys_lock);
    } else {
        exit(EXIT_BAD_PTR);
    }

    return bytes_read;
}

/* Writes size bytes from buffer to the open file fd. Returns the number of
 * bytes actually written, which may be less than size if some bytes could not
 * be written.
 */
int write(int fd, const void *buffer, unsigned size) {
    int bytes_written = -1;

    if (valid_user_pointer(buffer) && valid_user_pointer(buffer + size)) {
        if (fd == STDOUT_FILENO) {
            putbuf((const char *) buffer, (size_t) size);
        } else {
            lock_acquire(&filesys_lock);
            if (file_is_open(fd)) {
//              printf("Trying to write...\n");
                bytes_written = file_write(get_file_struct(fd), buffer, size);
            }
            lock_release(&filesys_lock);
        }
    } else {
        exit(EXIT_BAD_PTR);
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

//     printf("in close()\n");
    struct thread * cur_thread = thread_current();

    lock_acquire(&filesys_lock);
    if (file_is_open(fd)) {
        file_close(get_file_struct(fd));
        thread_current()->process_details->files[fd] = NULL;

        cur_thread->process_details->num_files_open--;
    }
    lock_release(&filesys_lock);
}
