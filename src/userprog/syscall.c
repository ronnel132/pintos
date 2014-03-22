#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
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
#include "filesys/directory.h"

#include "devices/input.h"

#include "userprog/process.h"

/* Frees a page */
extern void palloc_free_page (void *);

/* Validates a user-provided pointer. Checks that it's in the required
 * range, and that it correpsonds to a page directory entry
 */
static bool valid_user_pointer(const void *ptr);

/* Checks if a file is open */
static bool is_open(int fd);

/* Gets the file struct from a file descriptor */
static struct file * get_file_struct(int fd);
static struct dir * get_dir_struct(int fd);
static is_open_by_process(struct inode * inode);

/* Function prototype */
static void syscall_handler(struct intr_frame *);

/* Global thread lists */
extern struct list dead_list;
extern struct list all_list;


/* Validates a user-provided pointer. Checks that it's in the required
 * range, and that it correpsonds to a page directory entry
 */
static bool valid_user_pointer(const void *ptr) {
    uint32_t *pd, *pde;

    pd = active_pd();
    pde = pd + pd_no(ptr);

    /* See lookup_page() for more info */
    if (is_user_vaddr(ptr) && (*pde != 0)) {
        return true;
    }

    return false;
}


/* Helper functions for files related to process. */

/* Checks if a file descriptor is open */
static bool is_open(int fd) {
    bool opn = false;

    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        opn = thread_current()->process_details->open_file_descriptors[fd];
    }

    return opn;
}

/* Gets the file struct from a file descriptor */
static struct file * get_file_struct(int fd) {
    struct file *f = NULL;

    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        f = thread_current()->process_details->files[fd];
    }

    return f;
}

/* Gets the dir struct from a file descriptor */
static struct dir * get_dir_struct(int fd) {
    struct dir *d = NULL;

    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        d = thread_current()->process_details->directories[fd];
    }

    return d;
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
    /* Check validity of syscall_nr */
    if (!valid_user_pointer(f->esp)) {
        exit(EXIT_BAD_PTR);
    }

    /* Extract syscall numbers and arguments */
    int syscall_nr = *((int *)f->esp);

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
            f->eax = filesize(*((int *) arg1));
            break;

        case SYS_READ:
            if ((!valid_user_pointer(arg1)) ||
                (!valid_user_pointer(arg2)) ||
                (!valid_user_pointer(arg3)) ||
                (!valid_user_pointer(arg4))) {

                exit(EXIT_BAD_PTR);
            }
            f->eax = read(*((int *) arg1),
                          *((void **) arg2),
                          *((unsigned *) arg3));
            break;

        case SYS_WRITE:
            f->eax = write(*((int *) arg1),
                           *((void **) arg2),
                           *((unsigned *) arg3));
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
            f->eax = tell(*((int *) arg1));
            break;

        case SYS_CLOSE:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            close(*((int *) arg1));
            break;

        case SYS_CHDIR:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = chdir(*((const char **)arg1));
            break;

        case SYS_MKDIR:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = mkdir(*((const char **)arg1));
            break;

        case SYS_READDIR:
            if ((!valid_user_pointer(arg1)) || (!valid_user_pointer(arg2))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = readdir(*((int *)arg1), *((char **)arg2)); 
            break;

        case SYS_ISDIR:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = isdir(*((int *) arg1));
            break;

        case SYS_INUMBER:
            if ((!valid_user_pointer(arg1))) {
                exit(EXIT_BAD_PTR);
            }
            f->eax = inumber(*((int *) arg1));
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
    tid_t tid = -1;
    if (valid_user_pointer(cmd_line)) {
        tid = process_execute(cmd_line);

        /* Wait for process to actually load */
        sema_down(thread_current()->child_loaded_sema);

        if (thread_current()->child_loaded_error == 1) {
            tid = -1;
        }
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
    bool status;
    int name_length;
    char name[READDIR_MAX_LEN + 1];
    char *name_ptr;
    struct inode * opened_inode;
    struct inode * temp_inode;
    struct dir * cur_dir;

    /* Validate user pointer and then open file. */
    if (file == NULL || !valid_user_pointer(file)) {
        exit(EXIT_BAD_PTR);
    }

    /* Try and find inode corresponding to given file name string */
    name_ptr = file;

    if (*name_ptr == '/') {
        opened_inode = inode_open(ROOT_DIR_SECTOR);
        name_ptr++;
    }
    else if (*name_ptr != '\0') {
        opened_inode = inode_reopen(dir_inode(thread_cwd()));
    }
    else {
        return false;
    }

    inode_set_dir(opened_inode, true);

    name[READDIR_MAX_LEN] = '\0';
    name_length = 0;
    cur_dir = NULL;
    status = false;

    while (true) {
        /* If ending current token. */
        if (*name_ptr == '/' || *name_ptr == '\0') {
            name[name_length] = '\0';

            /* If token length is greater than 0. */
            if (name_length > 0) {
                if (strcmp(name, "..") == 0) {
                    if (inode_isdir(opened_inode)) {
                        temp_inode = opened_inode;
                        opened_inode = inode_parent(opened_inode);
                        inode_close(temp_inode);
                    }
                    else {
                        status = false;
                        break;
                    }
                }
                else if (strcmp(name, ".") == 0) {
                    if (!inode_isdir(opened_inode)) {
                        status = false;
                        break;
                    }
                }
                else {
                    if (inode_isdir(opened_inode)) {
                        cur_dir = dir_open(opened_inode);
                        if (dir_lookup(cur_dir, name, &opened_inode)) {
                            dir_close(cur_dir);
                        }
                        else if (*name_ptr == '\0') {
                            status = true;
                            break;
                        }
                        else {
                            status = false;
                            dir_close(cur_dir);
                            break;
                        }
                    }
                    else {
                        status = false;
                        break;
                    }
                }
            }

            name[0] = '\0';
            name_length = 0;

            /* End loop if at end of given string. */
            if (*name_ptr == '\0') {
                status = false;
                break;
            }
        }
        /* Else if building current token name. */
        else if (name_length < READDIR_MAX_LEN) {
            name[name_length] = *name_ptr;
            name_length++;
        }
        /* Else name length is overflowing. */
        else {
            status = false;
            break;
        }

        name_ptr++;
    }

    if (status) {
        block_sector_t inode_sector = 0;
        status = (cur_dir != NULL &&
                        free_map_allocate(1, &inode_sector) &&
                        inode_create(inode_sector, initial_size) &&
                        dir_add(cur_dir, name, inode_sector, false));
        if (!status && inode_sector != 0) {
            free_map_release(inode_sector, 1);
        }
        dir_close(cur_dir);
    }

    return status;
}

/* Deletes the file called file. Returns true if successful, false otherwise.
 */
bool remove(const char *file) {
    int name_length;
    char name[READDIR_MAX_LEN + 1];
    char *name_ptr;
    bool status;
    struct inode * opened_inode;
    struct inode * temp_inode;
    struct dir * cur_dir;

    /* Validate user pointer and then open file. */
    if (file == NULL || !valid_user_pointer(file)) {
        exit(EXIT_BAD_PTR);
    }

    if (strcmp(file, "/") == 0) {
        return false;
    }

    /* Try and find inode corresponding to given file name string */
    name_ptr = file;

    if (*name_ptr == '/') {
        opened_inode = inode_open(ROOT_DIR_SECTOR);
        name_ptr++;
    }
    else if (*name_ptr != '\0') {
        opened_inode = inode_reopen(dir_inode(thread_cwd()));
    }
    else {
        return false;
    }

    inode_set_dir(opened_inode, true);

    name[READDIR_MAX_LEN] = '\0';
    name_length = 0;
    cur_dir = NULL;
    status = false;

    while (true) {
        /* If ending current token. */
        if (*name_ptr == '/' || *name_ptr == '\0') {
            name[name_length] = '\0';

            /* If token length is greater than 0. */
            if (name_length > 0) {
                if (strcmp(name, "..") == 0) {
                    if (inode_isdir(opened_inode)) {
                        temp_inode = opened_inode;
                        opened_inode = inode_parent(opened_inode);
                        inode_close(temp_inode);
                    }
                    else {
                        status = false;
                        break;
                    }
                }
                else if (strcmp(name, ".") == 0) {
                    if (!inode_isdir(opened_inode)) {
                        status = false;
                        break;
                    }
                    /* else do nothing */
                }
                else {
                    if (inode_isdir(opened_inode)) {
                        cur_dir = dir_open(opened_inode);
                        if (*name_ptr == '\0') {
                            dir_lookup(cur_dir, name, &opened_inode);
                            if (inode_get_inumber(opened_inode) == inode_get_inumber(dir_inode(thread_cwd())) ||
                                inode_get_inumber(opened_inode) == ROOT_DIR_SECTOR ||
                                is_open_by_process(opened_inode)) {
                                status = false;
                            }
                            else {
                                status = dir_remove(cur_dir, name);
                            }
                            break;
                        }
                        else if (dir_lookup(cur_dir, name, &opened_inode)) {
                            dir_close(cur_dir);
                        }
                        else {
                            dir_close(cur_dir);
                            status = false;
                            break;
                        }
                    }
                    else {
                        status = false;
                        break;
                    }
                }
            }

            name[0] = '\0';
            name_length = 0;

            /* End loop if at end of given string. */
            if (*name_ptr == '\0') {
                status = true;
                break;
            }
        }
        /* Else if building current token name. */
        else if (name_length < READDIR_MAX_LEN) {
            name[name_length] = *name_ptr;
            name_length++;
        }
        /* Else name length is overflowing. */
        else {
            status = false;
            break;
        }

        name_ptr++;
    }

    return status;
}

/* Opens the file called file. Returns a nonnegative integer handle called 
 * a "file descriptor" (fd), or -1 if the file could not be opened. 
 */
int open(const char *file) {
    struct file * opened_file;
    struct dir * opened_dir;
    struct process * pd;
    int file_descriptor = -1;
    int i;
    
    int name_length;
    char name[READDIR_MAX_LEN + 1];
    char *name_ptr;
    bool open_fail;
    struct inode * opened_inode;
    struct inode * temp_inode;
    struct dir * cur_dir;

    /* Validate user pointer and then open file. */
    if (file == NULL || !valid_user_pointer(file)) {
        exit(EXIT_BAD_PTR);
    }

    /* Try and find inode corresponding to given file name string */
    name_ptr = file;

    if (*name_ptr == '/') {
        opened_inode = inode_open(ROOT_DIR_SECTOR);
        name_ptr++;
    }
    else if (*name_ptr != '\0') {
        opened_inode = inode_reopen(dir_inode(thread_cwd()));
    }
    else {
        return -1;
    }

    inode_set_dir(opened_inode, true);

    name[READDIR_MAX_LEN] = '\0';
    name_length = 0;
    cur_dir = NULL;
    open_fail = true;

    while (true) {
        /* If ending current token. */
        if (*name_ptr == '/' || *name_ptr == '\0') {
            name[name_length] = '\0';

            /* If token length is greater than 0. */
            if (name_length > 0) {
                if (strcmp(name, "..") == 0) {
                    if (inode_isdir(opened_inode)) {
                        temp_inode = opened_inode;
                        opened_inode = inode_parent(opened_inode);
                        inode_close(temp_inode);
                    }
                    else {
                        open_fail = true;
                        break;
                    }
                }
                else if (strcmp(name, ".") == 0) {
                    if (!inode_isdir(opened_inode)) {
                        open_fail = true;
                        break;
                    }
                    /* else do nothing */
                }
                else {
                    if (inode_isdir(opened_inode)) {
                        cur_dir = dir_open(opened_inode);
                        if (dir_lookup(cur_dir, name, &opened_inode)) {
                            dir_close(cur_dir);
                        }
                        else {
                            dir_close(cur_dir);
                            open_fail = true;
                            break;
                        }
                    }
                    else {
                        open_fail = true;
                        break;
                    }
                }
            }

            name[0] = '\0';
            name_length = 0;

            /* End loop if at end of given string. */
            if (*name_ptr == '\0') {
                open_fail = false;
                break;
            }
        }
        /* Else if building current token name. */
        else if (name_length < READDIR_MAX_LEN) {
            name[name_length] = *name_ptr;
            name_length++;
        }
        /* Else name length is overflowing. */
        else {
            open_fail = true;
            break;
        }

        name_ptr++;
    }


    if (open_fail) {
        inode_close_tree(opened_inode);
    }
    /* If inode was opened properly, open corresponding directory or file. */
    else {
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
            pd->num_files_open++;
            pd->fd_is_dir[file_descriptor] = inode_isdir(opened_inode);

            if (pd->fd_is_dir[file_descriptor]) {
                pd->directories[file_descriptor] = dir_open(opened_inode);
            }
            else {
                pd->files[file_descriptor] = file_open(opened_inode);
            }
        }
    }

    return file_descriptor;
}


/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd) {
    int size = -1;

    /* If file is indeed open, return length */
    if (is_open(fd) && !isdir(fd)) {
        size = file_length(get_file_struct(fd));
    }

    return size;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number
 * of bytes actually read (0 at end of file), or -1 if the file could not be
 * read (due to a condition other than end of file). Fd 0 reads from the
 * keyboard using input_getc().
 */
int read(int fd, void *buffer, unsigned size) {
    int bytes_read = -1;

    /* If the ENTIRE buffer is valid pointer range */
    if (valid_user_pointer(buffer) && valid_user_pointer(buffer + size)) {
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
        } else if (fd != STDOUT_FILENO && is_open(fd) && !isdir(fd)) {
            /* If the file descriptor is not stdout, read the file using
             * the filesystem.
             */
            bytes_read = file_read(get_file_struct(fd), buffer, size);
        }
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

    /* If the ENTIRE buffer is valid pointer range */
    if (valid_user_pointer(buffer) && valid_user_pointer(buffer + size)) {
        if (fd == STDOUT_FILENO) {
            /* If writing to stdout putbuf to output size bytes from buffer
             * to console.
             */
            putbuf((const char *) buffer, (size_t) size);
        } else if (fd != STDIN_FILENO && is_open(fd) && !isdir(fd)) {
            /* If file is not stdin, write to file in the filesystem */
            bytes_written = file_write(get_file_struct(fd), buffer, size);
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
    if (is_open(fd) && !isdir(fd)) {
        file_seek(get_file_struct(fd), position);
    }
}

/* Returns the position of the next byte to be read or written in open file
 * fd, expressed in bytes from the beginning of the file.
 */
unsigned tell(int fd) {
    unsigned pos = 0;

    if (is_open(fd) && !isdir(fd)) {
        pos = file_tell(get_file_struct(fd));
    }

    return pos;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly
 * closes all its open file descriptors, as if by calling this function for
 * each one.
 */
void close(int fd) {
    struct thread * cur_thread = thread_current();

    if (is_open(fd)) {
        /* If file is opened by this process */

        /* Update the process_details struct.
         * Specify that the filedescriptor is now available.
         */
        thread_current()->process_details->open_file_descriptors[fd] = false;

        /* Decrement the variable of number of files that are open */
        cur_thread->process_details->num_files_open--;


        if (cur_thread->process_details->fd_is_dir[fd]) {
            /* Close the directory using the filesystem. */
            dir_close(get_dir_struct(fd));

            /* Set the dir pointer at the file descriptor index in the
             * directories array to NULL.
             */
            thread_current()->process_details->directories[fd] = NULL;
        }
        else {
            /* Close the file using the filesystem. */
            file_close(get_file_struct(fd));

            /* Set the file pointer at the file descriptor index in the files
             * array to NULL.
             */
            thread_current()->process_details->files[fd] = NULL;
        }
    }
}

/* Changes the current working directory of the process to dir, which may be
 * relative or absolute. Returns true if successful, false on failure.
 */
bool chdir(const char *dir) {
    int name_length;
    char name[READDIR_MAX_LEN + 1];
    char *name_ptr;
    bool open_fail;
    struct inode * opened_inode;
    struct inode * temp_inode;
    struct dir * cur_dir;

    /* Validate user pointer and then open file. */
    if (dir == NULL || !valid_user_pointer(dir)) {
        exit(EXIT_BAD_PTR);
    }

    /* Try and find inode corresponding to given file name string */
    name_ptr = dir;

    if (*name_ptr == '/') {
        opened_inode = inode_open(ROOT_DIR_SECTOR);
        name_ptr++;
    }
    else if (*name_ptr != '\0') {
        opened_inode = inode_reopen(dir_inode(thread_cwd()));
    }
    else {
        return -1;
    }

    inode_set_dir(opened_inode, true);

    name[READDIR_MAX_LEN] = '\0';
    name_length = 0;
    cur_dir = NULL;
    open_fail = true;

    while (true) {
        /* If ending current token. */
        if (*name_ptr == '/' || *name_ptr == '\0') {
            name[name_length] = '\0';

            /* If token length is greater than 0. */
            if (name_length > 0) {
                if (strcmp(name, "..") == 0) {
                    if (inode_isdir(opened_inode)) {
                        temp_inode = opened_inode;
                        opened_inode = inode_parent(opened_inode);
                        inode_close(temp_inode);
                    }
                    else {
                        open_fail = true;
                        break;
                    }
                }
                else if (strcmp(name, ".") == 0) {
                    if (!inode_isdir(opened_inode)) {
                        open_fail = true;
                        break;
                    }
                    /* else do nothing */
                }
                else {
                    if (inode_isdir(opened_inode)) {
                        cur_dir = dir_open(opened_inode);
                        if (dir_lookup(cur_dir, name, &opened_inode)) {
                            dir_close(cur_dir);
                        }
                        else {
                            dir_close(cur_dir);
                            open_fail = true;
                            break;
                        }
                    }
                    else {
                        open_fail = true;
                        break;
                    }
                }
            }

            name[0] = '\0';
            name_length = 0;

            /* End loop if at end of given string. */
            if (*name_ptr == '\0') {
                open_fail = false;
                break;
            }
        }
        /* Else if building current token name. */
        else if (name_length < READDIR_MAX_LEN) {
            name[name_length] = *name_ptr;
            name_length++;
        }
        /* Else name length is overflowing. */
        else {
            open_fail = true;
            break;
        }

        name_ptr++;
    }

    inode_close_tree(opened_inode);

    if (open_fail || !inode_isdir(opened_inode)) {
        return false;
    }
    else {
        dir_close(thread_cwd());
        thread_current()->cwd = dir_open(opened_inode);
        return true;
    }
}

/* Creates the directory named dir, which may be relative or absolute.
 * Returns true if successful, false on failure. Fails if dir already
 * exists or if any directory name in dir, besides the last, does not
 * already exist. That is, mkdir("/a/b/c") succeeds only if /a/b already
 * exists and /a/b/c does not.
 */
bool mkdir(const char *dir) {
    bool status;
    int name_length;
    char name[READDIR_MAX_LEN + 1];
    char *name_ptr;
    struct inode * opened_inode;
    struct inode * temp_inode;
    struct dir * cur_dir;

    /* Validate user pointer and then open file. */
    if (dir == NULL || !valid_user_pointer(dir)) {
        exit(EXIT_BAD_PTR);
    }

    /* Try and find inode corresponding to given file name string */
    name_ptr = dir;

    if (*name_ptr == '/') {
        opened_inode = inode_open(ROOT_DIR_SECTOR);
        name_ptr++;
    }
    else if (*name_ptr != '\0') {
        opened_inode = inode_reopen(dir_inode(thread_cwd()));
    }
    else {
        return false;
    }

    inode_set_dir(opened_inode, true);

    name[READDIR_MAX_LEN] = '\0';
    name_length = 0;
    cur_dir = NULL;
    status = false;

    while (true) {
        /* If ending current token. */
        if (*name_ptr == '/' || *name_ptr == '\0') {
            name[name_length] = '\0';

            /* If token length is greater than 0. */
            if (name_length > 0) {
                if (strcmp(name, "..") == 0) {
                    if (inode_isdir(opened_inode)) {
                        temp_inode = opened_inode;
                        opened_inode = inode_parent(opened_inode);
                        inode_close(temp_inode);
                    }
                    else {
                        status = false;
                        break;
                    }
                }
                else if (strcmp(name, ".") == 0) {
                    if (!inode_isdir(opened_inode)) {
                        status = false;
                        break;
                    }
                }
                else {
                    if (inode_isdir(opened_inode)) {
                        cur_dir = dir_open(opened_inode);
                        if (dir_lookup(cur_dir, name, &opened_inode)) {
                            dir_close(cur_dir);
                        }
                        else if (*name_ptr == '\0') {
                            status = true;
                            break;
                        }
                        else {
                            status = false;
                            dir_close(cur_dir);
                            break;
                        }
                    }
                    else {
                        status = false;
                        break;
                    }
                }
            }

            name[0] = '\0';
            name_length = 0;

            /* End loop if at end of given string. */
            if (*name_ptr == '\0') {
                status = false;
                break;
            }
        }
        /* Else if building current token name. */
        else if (name_length < READDIR_MAX_LEN) {
            name[name_length] = *name_ptr;
            name_length++;
        }
        /* Else name length is overflowing. */
        else {
            status = false;
            break;
        }

        name_ptr++;
    }

    if (status) {
        block_sector_t inode_sector = 0;
        status = (cur_dir != NULL &&
                        free_map_allocate(1, &inode_sector) &&
                        dir_create(inode_sector, 0) &&
                        dir_add(cur_dir, name, inode_sector, true));
        if (!status && inode_sector != 0) {
            free_map_release(inode_sector, 1);
        }
        dir_close(cur_dir);
    }

    return status;
}

/* Reads a directory entry from file descriptor fd, which must represent a
 * directory. If successful, stores the null-terminated file name in name,
 * which must have room for READDIR_MAX_LEN + 1 bytes, and returns true. If
 * no entries are left in the directory, returns false.
 *
 * . and .. should not be returned by readdir.
 *
 * If the directory changes while it is open, then it is acceptable for some
 * entries not to be read at all or to be read multiple times. Otherwise,
 * each directory entry should be read once, in any order.
 *
 * READDIR_MAX_LEN is defined in lib/user/syscall.h. If your file system
 * supports longer file names than the basic file system, you should
 * increase this value from the default of 14.
 */
bool readdir(int fd, char *name) {
    return false;
}

/* Returns true if fd represents a directory, false if it represents an
 * ordinary file.
 */
bool isdir(int fd) {
    return thread_current()->process_details->fd_is_dir[fd];
}

/* Returns the inode number of the inode associated with fd, which may
 * represent an ordinary file or a directory.
 */
int inumber(int fd) {
    if (isdir(fd)) {
        return (int) inode_get_inumber(dir_get_inode(get_dir_struct(fd)));
    } else {
        return (int) inode_get_inumber(file_get_inode(get_file_struct(fd)));
    }
    return false;
}

static is_open_by_process(struct inode * inode) {
    struct process * pd = thread_current()->process_details;
    int i;


    /* Search for inode in open files and directories */
    for (i = 2; i < MAX_OPEN_FILES; i++) {
        if (pd->open_file_descriptors[i] == true) {
            if (inumber(i) == inode_get_inumber(inode)) {
                return true;
            }
        }
    }

    return false;
}


