/*! \file thread.h
 *
 * Declarations for the kernel threading functionality in PintOS.
 */

#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "devices/timer.h"
#include "threads/fixed-point.h"
#include "threads/synch.h"

/*! States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING,     /*!< Running thread. */
    THREAD_READY,       /*!< Not running but ready to run. */
    THREAD_BLOCKED,     /*!< Waiting for an event to trigger. */
    THREAD_DYING        /*!< About to be destroyed. */
};

/*! Thread identifier type.
    You can redefine this to whatever type you like. */
typedef int tid_t;
typedef int pid_t;
#define TID_ERROR ((tid_t) -1)          /*!< Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /*!< Lowest priority. */
#define PRI_DEFAULT 31                  /*!< Default priority. */
#define PRI_MAX 63                      /*!< Highest priority. */

#define NICE_MIN -20
#define NICE_MAX 20

/*! A process can have a max of 128 open files */
#define MAX_OPEN_FILES 128

/*! Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14


/*! Process struct used by the kernel to keep track of process specific
    information as opposed to thread specific information. */

struct process {
    tid_t parent_id;
    struct file * exec_file;
    

    int num_files_open;
    bool open_file_descriptors[MAX_OPEN_FILES];
    struct file * files[MAX_OPEN_FILES];
};

/*! A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

\verbatim
        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+
\endverbatim

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion.

   The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list.
*/

struct thread {
    /*! Owned by thread.c. */
    /**@{*/
    tid_t tid;                          /*!< Thread identifier. */
    enum thread_status status;          /*!< Thread state. */
    char name[16];                      /*!< Name (for debugging purposes). */
    uint8_t *stack;                     /*!< Saved stack pointer. */
    int priority;                       /*!< Priority. */
    int donation_priority;              /*!< Donation priority (-1 if N/A). */
    int niceness;           /*!< Between -20 and 20. */
    fixedpt recent_cpu;         /*!< CPU usage recently. */
    struct list_elem allelem;           /*!< List element for all threads list. */
    /**@}*/


    /* The thread this thread donates to. NULL if this thread doesn't donate
       to any thread. */
    struct thread *donee;

    /*! Shared between thread.c and synch.c. */
    /**@{*/
    struct list_elem elem;              /*!< List element. */
    /**@}*/

#ifdef USERPROG
    int exit_status;
  
    /* Process information */
    struct process * process_details;

    /* The process current directory. */
    struct dir *cwd;

    /*! Owned by userprog/process.c. */
    /**@{*/
    uint32_t *pagedir;                  /*!< Page directory. */

    /* Semaphore used by waiter. Parent downs this semaphore when he wait()s
     * for this child, and the child ups it when he exits
     */
    struct semaphore *waiter_sema;
    /**@}*/

    /* A semaphore to synch whether a child has been loaded */
    struct semaphore *child_loaded_sema;

    /* This will be 1 if there's a child load error, otherwise undefined */
    int child_loaded_error;
#endif

    /*! Owned by thread.c. */
    /**@{*/
    unsigned magic;                     /* Detects stack overflow. */
    /**@}*/
};

struct priority_donation_state {
    /* The lock that the current thread is seeking to unlock so that the 
     * thread running previously can resume execution.
     */
    struct lock *lock_desired;
    struct thread *donor;
  struct thread *donee;
    struct list_elem elem;              /*!< List element. */
};

/* Keeps track of the state of current running threads under the MLFQ 
   option. */ 
struct thread_mlfq_state {
  struct list *ready_lists;
  /* Keep track of the highest priority non-empty list in ready_lists, so 
     that we may access it quickly. */
  int highest_non_empty_list;
  /* Keep track of the number of ready threads in all of the ready_lists. */
  int num_ready;
};

/*! List of priority donation states, for keeping track of chains of priority 
    donations (i.e., A donates to B, B donates to C, etc.). When not exploring
    some chain of priority donations, this linked list is empty. */
extern struct list pri_donation_list;


/* Processes that are dead but haven't been reaped yet */
#ifdef USERPROG
extern struct list dead_list;
#endif


/*! A sleeping kernel thread.
    Stores the thread struct pointer and the time it
    should be unblocked.
 */
struct thread_sleeping {
  /* Thread pointer. */
  struct thread *t;

  /* End time. Compare to timer_ticks(). */
  int64_t end_ticks;

  /* List element for sleep list */
  struct list_elem elem;
};


#ifdef USERPROG
/*! A waitee kernel thread, i.e. a thread for which its parent is waiting */
struct thread_dead {
    /* Thread ID. */
    tid_t tid;

    /* Parent id */
    tid_t parent_id;

    /* Exit status */
    int status;

    /* Semaphore used by waiter. Parent downs this semaphore when he wait()s
     * for this child, and the child ups it when he exits
     */
    struct semaphore *waiter_sema;

    /* List element for dead list */
    struct list_elem elem;
};
#endif



/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
bool ready_less(const struct list_elem *elem1, const struct list_elem *elem2,
          void *aux);
void thread_unblock(struct thread *);

struct thread *thread_current (void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);
bool thread_sleep_less(const struct list_elem *elem1, 
             const struct list_elem *elem2, void *aux);
void thread_sleep(int64_t end_ticks);

/*! Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);

void thread_foreach(thread_action_func *, void *);

int thread_get_priority(void);
void thread_set_priority(int);

bool get_thread_mlfqs(void);

/* Schedules the donor of this thread, and sets the priority of the
 * current thread to its original priority
 */
void schedule_donor(void);

/* Donate current thread's priority to donee */
void donate_priority(struct thread *donee);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

/* Calculates the max of two numbers */
int max(int a, int b);

/* Returns the effective priority */
int effective_priority(struct thread *t);

/* Return max priority of the list */
int max_ready_priority(void);

#endif /* threads/thread.h */

