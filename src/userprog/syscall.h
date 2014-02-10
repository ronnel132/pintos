#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

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


#endif /* userprog/syscall.h */

