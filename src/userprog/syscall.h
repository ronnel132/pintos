#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

/*! Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0          /*!< Successful execution. */
#define EXIT_FAILURE 1          /*!< Unsuccessful execution. */
#define EXIT_BAD_PTR -1         /*!< Bad pointer access during execution. */


/*! Lock used by filesystem syscalls. */
extern struct lock filesys_lock;

/* Installs the syscall handler into the interrupt vector table. */
void syscall_init(void);

/* Terminates Pintos by calling shutdown_power_off() (declared in 
 * "devices/shutdown.h"). This should be seldom used, because you 
 * lose some information about possible deadlock situations, etc. 
 */
void halt(void);

/* Terminates the current user program, returning status to the kernel. */
void exit(int status);

/* Runs the executable whose name is given in cmd_line, passing any given 
 * arguments, and returns the new process's program id (pid).
 */
pid_t exec(const char *cmd_line);

/* Waits for a child process pid and retrieves the child's exit status. */
int wait(pid_t pid);

/* Creates a new file called file initially initial_size bytes in size. 
 * Returns true if successful, false otherwise. 
 */
bool create(const char *file, unsigned initial_size);

/* Deletes the file called file. Returns true if successful, false otherwise.
 */
bool remove(const char *file);

/* Opens the file called file. Returns a nonnegative integer handle called 
 * a "file descriptor" (fd), or -1 if the file could not be opened. 
 */
int open(const char *file);

/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd);

/* Reads size bytes from the file open as fd into buffer. Returns the number
 * of bytes actually read (0 at end of file), or -1 if the file could not be
 * read (due to a condition other than end of file). Fd 0 reads from the
 * keyboard using input_getc().
 */
int read(int fd, void *buffer, unsigned size);

/* Writes size bytes from buffer to the open file fd. Returns the number of
 * bytes actually written, which may be less than size if some bytes could not
 * be written.
 */
int write(int fd, const void *buffer, unsigned size);

/* Changes the next byte to be read or written in open file fd to position,
 * expressed in bytes from the beginning of the file. (Thus, a position of 0
 * is the file's start.)
 */
void seek(int fd, unsigned position);

/* Returns the position of the next byte to be read or written in open file
 * fd, expressed in bytes from the beginning of the file.
 */
unsigned tell(int fd);

/* Closes file descriptor fd. Exiting or terminating a process implicitly
 * closes all its open file descriptors, as if by calling this function for
 * each one.
 */
 void close(int fd);

/* Changes the current working directory of the process to dir, which may be
 * relative or absolute. Returns true if successful, false on failure.
 */
bool chdir(const char *dir);

/* Creates the directory named dir, which may be relative or absolute.
 * Returns true if successful, false on failure. Fails if dir already
 * exists or if any directory name in dir, besides the last, does not
 * already exist. That is, mkdir("/a/b/c") succeeds only if /a/b already
 * exists and /a/b/c does not.
 */
bool mkdir(const char *dir);

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
bool readdir(int fd, char *name);

/* Returns true if fd represents a directory, false if it represents an
 * ordinary file.
 */
bool isdir (int fd);

/* Returns the inode number of the inode associated with fd, which may
 * represent an ordinary file or a directory.
 */
int inumber(int fd);

#endif /* userprog/syscall.h */
