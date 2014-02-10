#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Installs the syscall handler into the interrupt vector table. */
void syscall_init(void);

/* Terminates Pintos by calling shutdown_power_off() (declared in 
 * "devices/shutdown.h"). This should be seldom used, because you 
 * lose some information about possible deadlock situations, etc. 
 */
void halt (void);

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
bool remove(const char (file);

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

#endif /* userprog/syscall.h */
