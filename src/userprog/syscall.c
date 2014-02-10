#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

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
    

}

/* Runs the executable whose name is given in cmd_line, passing any given 
 * arguments, and returns the new process's program id (pid).
 */
pid_t exec(const char *cmd_line);

/* Waits for a child process pid and retrieves the child's exit status. */
int wait(pid_t pid) {
    
}

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
