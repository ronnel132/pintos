#include "threads/thread.h" 
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/*! Random value for struct thread's `magic' member.
    Used to detect stack overflow.  See the big comment at the top
    of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/*! List of processes in THREAD_READY state, that is, processes
    that are ready to run but not actually running. */
static struct list ready_list;

/*! List of processes in THREAD_BLOCKED state and sleeping,
    that is, blocked processes that should be woken up after
    at a specified time. */
static struct list sleep_list;

/*! List of all processes.  Processes are added to this list
    when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Declare pri_donation_list struct from the header file definition */
struct list pri_donation_list;

/*! Idle thread. */
static struct thread *idle_thread;

/*! Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/*! Lock used by allocate_tid(). */
static struct lock tid_lock;

/*! Stack frame for kernel_thread(). */
struct kernel_thread_frame {
    void *eip;                  /*!< Return address. */
    thread_func *function;      /*!< Function to call. */
    void *aux;                  /*!< Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;    /*!< # of timer ticks spent idle. */
static long long kernel_ticks;  /*!< # of timer ticks in kernel threads. */
static long long user_ticks;    /*!< # of timer ticks in user programs. */

/* Global system load average. Initialized to zero. */
static fixedpt load_avg = 0;

static void recalculate_priority(struct thread *t);
static void recalculate_recent_cpu(struct thread *t);
static void recalculate_load_avg();

/* Scheduling. */
#define TIME_SLICE 4            /*!< # of timer ticks to give each thread. */
static unsigned thread_ticks;   /*!< # of timer ticks since last yield. */

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *running_thread(void);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static bool is_thread(struct thread *) UNUSED;
static void *alloc_frame(struct thread *, size_t size);
static void schedule(void);
void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);


/* Calculates the max of two numbers */
int max(int a, int b) {
    if (a > b)
        return a;
    return b;
}

/* Returns the effective priority */
int effective_priority(struct thread *t) {
    if (!thread_mlfqs) {
        return max(t->priority, t->donation_priority);
    }
    else {
        return t->priority;
    }
}

/* Return max priority of the list */
int max_ready_priority() {
    return effective_priority(list_entry(list_begin(&ready_list), 
        struct thread, elem));
}

/*! Initializes the threading system by transforming the code
    that's currently running into a thread.  This can't work in
    general and it is possible in this case only because loader.S
    was careful to put the bottom of the stack at a page boundary.

    Also initializes the run queue and the tid lock.

    After calling this function, be sure to initialize the page allocator
    before trying to create any threads with thread_create().

    It is not safe to call thread_current() until this function finishes. */
void thread_init(void) {
    ASSERT(intr_get_level() == INTR_OFF);

    lock_init(&tid_lock);

    list_init(&ready_list);

    list_init(&sleep_list);
    list_init(&all_list);
    list_init(&pri_donation_list);

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread();
    init_thread(initial_thread, "main", PRI_DEFAULT);
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid();
}

/*! Starts preemptive thread scheduling by enabling interrupts.
    Also creates the idle thread. */
void thread_start(void) {
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init(&idle_started, 0);
    thread_create("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down(&idle_started);
}

static void recalculate_priority(struct thread *t) {
    ASSERT (thread_get_nice() >= NICE_MIN 
        && thread_get_nice() <= NICE_MAX);
    ASSERT (thread_get_priority() >= PRI_MIN 
        && thread_get_priority() <= PRI_MAX);
    ASSERT (t->priority >= PRI_MIN && t->priority <= PRI_MAX);
    ASSERT (t->niceness >= NICE_MIN && t->niceness <= NICE_MAX);

    /* If not idle */
    if (t->tid != 2) {
//     if (strcmp(t->name, "idle") != 0) {
        t->priority = fixedpt_to_int_zero( 
                      fixedpt_sub(fixedpt_sub(int_to_fixedpt(PRI_MAX), 
                      fixedpt_div(t->recent_cpu, int_to_fixedpt(4))),
                      fixedpt_mul(int_to_fixedpt(t->niceness), 
                                  int_to_fixedpt(2))));

        /* Adjust priority to within our range */
        if (t->priority > PRI_MAX) {
            t->priority = PRI_MAX;
        }
        else if (t->priority < PRI_MIN) {
            t->priority = PRI_MIN;
        }
    }
        
    ASSERT (thread_get_priority() >= PRI_MIN 
        && thread_get_priority() <= PRI_MAX);
    ASSERT (t->priority >= PRI_MIN && t->priority <= PRI_MAX);
    ASSERT (t->niceness >= NICE_MIN && t->niceness <= NICE_MAX);
}

static void recalculate_recent_cpu(struct thread *t) {
    ASSERT (t->priority >= PRI_MIN && t->priority <= PRI_MAX);
    ASSERT (t->niceness >= NICE_MIN && t->niceness <= NICE_MAX);
    ASSERT (thread_get_nice() >= NICE_MIN && thread_get_nice() <= NICE_MAX);

    /* If not idle */
    if (t->tid != 2) {
//     if (strcmp(t->name, "idle") != 0) {
        /* Calculate recent_cpu, using fixed point arithmetic. */
        fixedpt fp2 = int_to_fixedpt(2);
        fixedpt fp1 = int_to_fixedpt(1);
        fixedpt coeff = fixedpt_div(fixedpt_mul(fp2, load_avg), 
                                fixedpt_add(fixedpt_mul(fp2, load_avg), fp1));
        t->recent_cpu = fixedpt_add(fixedpt_mul(coeff, t->recent_cpu), 
                                int_to_fixedpt(t->niceness)); 
    }
    ASSERT (thread_get_priority() >= PRI_MIN 
        && thread_get_priority() <= PRI_MAX);
    ASSERT (t->priority >= PRI_MIN && t->priority <= PRI_MAX);
    ASSERT (t->niceness >= NICE_MIN && t->niceness <= NICE_MAX);
}

static void recalculate_load_avg() {
    ASSERT (thread_get_priority() >= PRI_MIN 
        && thread_get_priority() <= PRI_MAX);
    ASSERT (thread_get_nice() >= NICE_MIN && thread_get_nice() <= NICE_MAX);
    fixedpt ready;
    static int i = 0;
    if (strcmp(thread_current()->name, "idle") == 0) {
        ready = int_to_fixedpt(list_size(&ready_list));
    } else {
        ready = int_to_fixedpt(list_size(&ready_list) + 1);
    }
    fixedpt fp59 = int_to_fixedpt(59);
    fixedpt fp60 = int_to_fixedpt(60);
    fixedpt fp1 = int_to_fixedpt(1);

    load_avg = fixedpt_add(fixedpt_mul(fixedpt_div(fp59, fp60), load_avg),
               fixedpt_mul(fixedpt_div(fp1, fp60), ready));

    ASSERT (thread_get_priority() >= PRI_MIN 
        && thread_get_priority() <= PRI_MAX);
}

/*! Called by the timer interrupt handler at each timer tick.
    Thus, this function runs in an external interrupt context. */
void thread_tick(void) {
    struct thread *t = thread_current();
    struct thread *iter_thread;
    struct list_elem *e;
    struct thread_sleeping *current_sleeper;
    int64_t current_ticks;
    int max_sleeper_pri = -1;
    int max_pri = -1;



    ASSERT(intr_context());

    /* Update statistics. */
    if (t == idle_thread)
        idle_ticks++;
#ifdef USERPROG
    else if (t->pagedir != NULL)
        user_ticks++;
#endif
    else
        kernel_ticks++;

    current_ticks = timer_ticks();

	if (thread_mlfqs) {
        
        /* Recalculate priority for all threads every fourth clock tick. */
        if (current_ticks % 4 == 0) {
            for (e = list_begin(&all_list); e != list_end(&all_list);
                 e = list_next(e)) {
                iter_thread = list_entry(e, struct thread, allelem);
                recalculate_priority(iter_thread);
                if (iter_thread->priority > max_pri)
                    max_pri = iter_thread->priority;
            }

            /* Recalculate recent_cpu and load_avg every second */
            if (current_ticks % TIMER_FREQ == 0) {
                /* This should occur ever TIMER_FREQ ticks (every second). */
                /* Iterate through all threads (running, ready or blocked) and 
                   recalculate recent_cpu. */
                for (e = list_begin(&all_list); e != list_end(&all_list);
                     e = list_next(e)) {
                    iter_thread = list_entry(e, struct thread, allelem);
                    recalculate_recent_cpu(iter_thread);
                }

                /* Update load_avg. */  
                recalculate_load_avg();
            }
        }

        if (t != idle_thread) {
            /* Increment recent_cpu with fixed point arithmetic. */
            t->recent_cpu = fixedpt_add(t->recent_cpu, int_to_fixedpt(1));
        }
    }

    /* Unblock all sleeping threads that need to be woken up. */
    e = list_begin(&sleep_list);
    while (e != list_end(&sleep_list)) {
        current_sleeper = list_entry(e, struct thread_sleeping, elem);
        if (current_sleeper->end_ticks > current_ticks) {
            break;
        }
        if (effective_priority(current_sleeper->t) > max_sleeper_pri)
            max_sleeper_pri = max(effective_priority(current_sleeper->t), max_pri);
        list_remove(&current_sleeper->elem);    
        thread_unblock(current_sleeper->t);

        e = list_next(e);
    }
    
    /* Enforce preemption. */
    if (++thread_ticks >= TIME_SLICE || 
        max_sleeper_pri >= thread_get_priority())
        intr_yield_on_return();

}

/*! Prints thread statistics. */
void thread_print_stats(void) {
    printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
           idle_ticks, kernel_ticks, user_ticks);
}

/*! Creates a new kernel thread named NAME with the given initial PRIORITY,
    which executes FUNCTION passing AUX as the argument, and adds it to the
    ready queue.  Returns the thread identifier for the new thread, or
    TID_ERROR if creation fails.

    If thread_start() has been called, then the new thread may be scheduled
    before thread_create() returns.  It could even exit before thread_create()
    returns.  Contrariwise, the original thread may run for any amount of time
    before the new thread is scheduled.  Use a semaphore or some other form of
    synchronization if you need to ensure ordering.

    The code provided sets the new thread's `priority' member to PRIORITY, but
    no actual priority scheduling is implemented.  Priority scheduling is the
    goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority, thread_func *function,
                    void *aux) {
    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;

    ASSERT(function != NULL);

    /* Allocate thread. */
    t = palloc_get_page(PAL_ZERO);
    if (t == NULL)
        return TID_ERROR;

    /* Initialize thread. */
    init_thread(t, name, priority);
    tid = t->tid = allocate_tid();
    t->process_details->parent_id = tid;

    /* Stack frame for kernel_thread(). */
    kf = alloc_frame(t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame(t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame(t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    /* Add to run queue. */
    thread_unblock(t);
    
    /* If this thread's priority is higher than or equal than the 
     * running thread's priority, yield the processor
     */
    if (effective_priority(t) >= thread_get_priority()) {
        /* Yield current thread, as the created thread has higher priority */
        thread_yield();
    }

    return tid;
}

/*! Puts the current thread to sleep.  It will not be scheduled
    again until awoken by thread_unblock().

    This function must be called with interrupts turned off.  It is usually a
    better idea to use one of the synchronization primitives in synch.h. */
void thread_block(void) {
    ASSERT(!intr_context());
    ASSERT(intr_get_level() == INTR_OFF);

    thread_current()->status = THREAD_BLOCKED;

    schedule();
}

/*! The ready LESS function, as required by the list_insert_ordered function,
    since the ready_list will be an ordered list. Used for comparing if one
    thread struct is LESS than the other, by comparing priority values. */
bool ready_less(const struct list_elem *elem1, const struct list_elem *elem2,
                void *aux) {
    struct thread *t1, *t2;
    t1 = list_entry(elem1, struct thread, elem);
    t2 = list_entry(elem2, struct thread, elem);

    ASSERT((t1->priority >= PRI_MIN) && (t1->priority <= PRI_MAX));
    ASSERT((t2->priority >= PRI_MIN) && (t2->priority <= PRI_MAX));

    /* We compare in this way so that if t1's priority is greater than t2's,
     * we will ensure that t1 will be placed before (closer to the HEAD of 
     * the ready queue) than t2.
     */
    return effective_priority(t1) >=  effective_priority(t2);
}

/*! Transitions a blocked thread T to the ready-to-run state.  This is an
    error if T is not blocked.  (Use thread_yield() to make the running
    thread ready.)

    This function does not preempt the running thread.  This can be important:
    if the caller had disabled interrupts itself, it may expect that it can
    atomically unblock a thread and update other data. */
void thread_unblock(struct thread *t) {
    enum intr_level old_level;

    ASSERT(is_thread(t));

    old_level = intr_disable();
    ASSERT(t->status == THREAD_BLOCKED);

    list_insert_ordered(&ready_list, &t->elem, &ready_less, NULL);

    t->status = THREAD_READY;

    /* Make sure the list is ordered */
    if (!get_thread_mlfqs()) {
        ASSERT(list_sorted(&ready_list, &ready_less, NULL));
    }
    intr_set_level(old_level);
}

/*! Returns the name of the running thread. */
const char * thread_name(void) {
    return thread_current()->name;
}

/*! Returns the running thread.
    This is running_thread() plus a couple of sanity checks.
    See the big comment at the top of thread.h for details. */
struct thread * thread_current(void) {
    struct thread *t = running_thread();

    /* Make sure T is really a thread.
       If either of these assertions fire, then your thread may
       have overflowed its stack.  Each thread has less than 4 kB
       of stack, so a few big automatic arrays or moderate
       recursion can cause stack overflow. */
    ASSERT(is_thread(t));
    ASSERT(t->status == THREAD_RUNNING);

    return t;
}

/*! Returns the running thread's tid. */
tid_t thread_tid(void) {
    return thread_current()->tid;
}

/*! Deschedules the current thread and destroys it.  Never
    returns to the caller. */
void thread_exit(void) {
    ASSERT(!intr_context());

#ifdef USERPROG
    process_exit();
#endif

    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it calls thread_schedule_tail(). */
    intr_disable();
    list_remove(&thread_current()->allelem);
    thread_current()->status = THREAD_DYING;
    schedule();
    NOT_REACHED();
}

/*! Yields the CPU.  The current thread is not put to sleep and
    may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void) {
    struct thread *cur = thread_current();
    struct thread *cur_ready;
    struct list_elem *cur_ready_elem;
    enum intr_level old_level;
    int i = 0;

    ASSERT(!intr_context());

    old_level = intr_disable();

    if (cur != idle_thread) {
        /* If there are other threads with the same priority as the thread 
           we are currently yielding, then place the current running thread 
           behind those threads, per round-robin rules. */
        if (!list_empty(&ready_list)) {
            cur_ready_elem = list_begin(&ready_list);
            cur_ready = list_entry(cur_ready_elem, struct thread, elem);
            if (effective_priority(cur_ready) == effective_priority(cur)) {
                i = 0;
                while (effective_priority(cur_ready) == effective_priority(cur)
                        &&  i < list_size(&ready_list)) {
                    i++;
                    cur_ready_elem = list_next(cur_ready_elem);
                    cur_ready = list_entry(cur_ready_elem, struct thread, elem);
                }
                list_insert(cur_ready_elem, &cur->elem);
            }
            else {
                list_insert_ordered(&ready_list, &cur->elem, &ready_less, NULL);
            }
        }
        else {
            list_insert_ordered(&ready_list, &cur->elem, &ready_less, NULL);
        }
    }

    cur->status = THREAD_READY;
    schedule();
    intr_set_level(old_level);
}

/*! LESS function provided to list_insert_ordered for the sleep_list. */
bool thread_sleep_less(const struct list_elem *elem1, 
                       const struct list_elem *elem2, void *aux) {
    /* The aux pointer is required, however we don't have need any auxiliary
     * data to perform our LESS comparison.
     */
    struct thread_sleeping *st1, *st2;
    st1 = list_entry(elem1, struct thread_sleeping, elem);
    st2 = list_entry(elem2, struct thread_sleeping, elem);

    /* Return true if st1->end_ticks <= st2->end_ticks. */
    return (st1->end_ticks <= st2->end_ticks);
}

/* Puts a thread to sleep until end_ticks */
void thread_sleep(int64_t end_ticks) {
    struct thread *t = thread_current();
    struct thread_sleeping *st = palloc_get_page(PAL_ZERO);
    enum intr_level old_level;

    /* This shouldn't be called on an interrupt context */
    ASSERT(!intr_context());

    /* Disable interrupts, as we shouldn't be interrupted here.
     * We're dealing with running/blocked queues, hence we are 
     * modifying global state.
     */
    old_level = intr_disable();

    if (st == NULL) {
        thread_yield();
    }
    else {
        st->t = t;
        st->end_ticks = end_ticks;
        
        /* Pass in NULL for auxiliary data pointer AUX. */
        list_insert_ordered(&sleep_list, &st->elem, &thread_sleep_less, NULL);

        /* Make sure the list is ordered */
        if (!get_thread_mlfqs()) {
            ASSERT(list_sorted(&ready_list, &ready_less, NULL));
        }

    }
    
    /* Take this thread off the ready list */
    thread_block();
    intr_set_level(old_level);
}




/*! Invoke function 'func' on all threads, passing along 'aux'.
    This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux) {
    struct list_elem *e;

    ASSERT(intr_get_level() == INTR_OFF);

    for (e = list_begin(&all_list); e != list_end(&all_list);
         e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);
        func(t, aux);
    }
}

/*! Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
    struct thread *max;
    enum intr_level old_level;
    
    ASSERT((new_priority >= PRI_MIN) && (new_priority <= PRI_MAX));
    old_level = intr_disable();

    thread_current()->priority = new_priority;

    /* Make sure the list is ordered */
    if (!get_thread_mlfqs()) {
        ASSERT(list_sorted(&ready_list, &ready_less, NULL));
    }

    /* Max priority thread */
    max = list_entry(list_begin(&ready_list), struct thread, elem);

    /* If the max thread is not the current thread */
    if (max != thread_current()) {
        thread_yield();
    }

    intr_set_level(old_level);
}

bool get_thread_mlfqs(void) {
    return thread_mlfqs;
}

/*! Returns the current thread's priority. */
int thread_get_priority(void) {
    return effective_priority(thread_current());
}

/*! Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice UNUSED) {
    struct thread *cur = thread_current();
    struct thread *next_ready;
    struct list_elem *next_ready_elem;
    enum intr_level old_level;

    cur->niceness = nice;
    ASSERT (thread_get_nice() >= NICE_MIN && thread_get_nice() <= NICE_MAX);
    old_level = intr_disable();


    recalculate_priority(cur);
    if (list_size(&ready_list) > 0) {
        next_ready_elem = list_begin(&ready_list);
        next_ready = list_entry(next_ready_elem, struct thread, elem);
        
        /* If the current thread is no longer the highest priority thread,
           then yield. */
        if (next_ready->priority > cur->priority) {
            thread_yield();
        }
    }
    intr_set_level(old_level);
}

/*! Returns the current thread's nice value. */
int thread_get_nice(void) {
    return thread_current()->niceness;
}

/*! Returns 100 times the system load average. */
int thread_get_load_avg(void) {
    return fixedpt_to_int_nearest(fixedpt_mul(int_to_fixedpt(100), 
           load_avg));
}

/*! Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
    return fixedpt_to_int_nearest(fixedpt_mul(int_to_fixedpt(100),
           thread_current()->recent_cpu));
}

/*! Idle thread.  Executes when no other thread is ready to run.

    The idle thread is initially put on the ready list by thread_start().
    It will be scheduled once initially, at which point it initializes
    idle_thread, "up"s the semaphore passed to it to enable thread_start()
    to continue, and immediately blocks.  After that, the idle thread never
    appears in the ready list.  It is returned by next_thread_to_run() as a
    special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED) {
    struct semaphore *idle_started = idle_started_;
    idle_thread = thread_current();
    sema_up(idle_started);

    for (;;) {
        /* Let someone else run. */
        intr_disable();
        thread_block();

        /* Re-enable interrupts and wait for the next one.

           The `sti' instruction disables interrupts until the completion of
           the next instruction, so these two instructions are executed
           atomically.  This atomicity is important; otherwise, an interrupt
           could be handled between re-enabling interrupts and waiting for the
           next one to occur, wasting as much as one clock tick worth of time.

           See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
           7.11.1 "HLT Instruction". */
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/*! Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux) {
    ASSERT(function != NULL);

    intr_enable();       /* The scheduler runs with interrupts off. */
    function(aux);       /* Execute the thread function. */
    thread_exit();       /* If function() returns, kill the thread. */
}

/*! Returns the running thread. */
struct thread * running_thread(void) {
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
       down to the start of a page.  Because `struct thread' is
       always at the beginning of a page and the stack pointer is
       somewhere in the middle, this locates the curent thread. */
    asm ("mov %%esp, %0" : "=g" (esp));
    return pg_round_down(esp);
}

/*! Returns true if T appears to point to a valid thread. */
static bool is_thread(struct thread *t) {
    return t != NULL && t->magic == THREAD_MAGIC;
}

/*! Does basic initialization of T as a blocked thread named NAME. */
static void init_thread(struct thread *t, const char *name, int priority) {
    enum intr_level old_level;

    ASSERT(t != NULL);
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT(name != NULL);

    memset(t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy(t->name, name, sizeof t->name);
    t->donee = NULL;
    t->stack = (uint8_t *) t + PGSIZE;
    t->priority = priority;
    t->donation_priority = -1;

    /* Addd process details */
    t->process_details = malloc(sizeof(struct process));
    t->process_details->num_files_open = 0;
	
    if (thread_mlfqs) {
        /* If we're in the first thread, set recent_cpu to 0, otherwise set to
           current thread's recent_cpu. */
        t->recent_cpu = int_to_fixedpt(0);
        if (strcmp(name, "main") == 0 || strcmp(t->name, "idle") == 0) {
            t->niceness = 0;
        }
        else {
//             t->recent_cpu = thread_current()->recent_cpu;
            t->niceness = thread_get_nice();
        }
    }

    t->magic = THREAD_MAGIC;

    old_level = intr_disable();
    list_push_back(&all_list, &t->allelem);
    intr_set_level(old_level);
}

/*! Allocates a SIZE-byte frame at the top of thread T's stack and
    returns a pointer to the frame's base. */
static void * alloc_frame(struct thread *t, size_t size) {
    /* Stack data is always allocated in word-size units. */
    ASSERT(is_thread(t));
    ASSERT(size % sizeof(uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/*! Chooses and returns the next thread to be scheduled.  Should return a
    thread from the run queue, unless the run queue is empty.  (If the running
    thread can continue running, then it will be in the run queue.)  If the
    run queue is empty, return idle_thread. */
static struct thread * next_thread_to_run(void) {
    struct list_elem *e;
    int max_pri = -1;
    struct thread *max_thread, *iter;

    ASSERT(intr_get_level() == INTR_OFF);

    /* Only assert sorted for NON mlfqs, other approach is manual in schedule */
    if (!get_thread_mlfqs()) {
        ASSERT(list_sorted(&ready_list, &ready_less, NULL));
    }

    if (list_empty(&ready_list)) {
        return idle_thread;
    } 
    if (!thread_mlfqs) {    
        return list_entry(list_pop_front(&ready_list), struct thread, elem);
    }
    else {
        /* Loop through all threads and find the one that has the max
         * priority. Return that one to get scheduled.
         */
        for (e = list_begin(&ready_list); e != list_end(&ready_list); 
             e = list_next(e)) {
            iter = list_entry(e, struct thread, elem);
            if (iter->priority > max_pri) {
                max_pri = iter->priority;
                max_thread = iter;
            } 
        }
        list_remove(&max_thread->elem);
        return max_thread;
    }
}

/*! Completes a thread switch by activating the new thread's page tables, and,
    if the previous thread is dying, destroying it.

    At this function's invocation, we just switched from thread PREV, the new
    thread is already running, and interrupts are still disabled.  This
    function is normally invoked by thread_schedule() as its final action
    before returning, but the first time a thread is scheduled it is called by
    switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is complete.  In
   practice that means that printf()s should be added at the end of the
   function.

   After this function and its caller returns, the thread switch is complete. */
void thread_schedule_tail(struct thread *prev) {
    struct thread *cur = running_thread();
  
    ASSERT(intr_get_level() == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
    process_activate();
#endif

    /* If the thread we switched from is dying, destroy its struct thread.
       This must happen late so that thread_exit() doesn't pull out the rug
       under itself.  (We don't free initial_thread because its memory was
       not obtained via palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING &&
        prev != initial_thread) {
        ASSERT(prev != cur);
        palloc_free_page(prev);
    }
}

/*! Schedules a new process.  At entry, interrupts must be off and the running
    process's state must have been changed from running to some other state.
    This function finds another thread to run and switches to it.

    It's not safe to call printf() until thread_schedule_tail() has
    completed. */
static void schedule(void) {
    struct thread *cur = running_thread();
    struct thread *next = next_thread_to_run();
    struct thread *prev = NULL;

    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(cur->status != THREAD_RUNNING);
    ASSERT(is_thread(next));

    /* Only assert sorted for NON mlfqs, other approach is manual in schedule */
    if (!get_thread_mlfqs()) {
        ASSERT(list_sorted(&ready_list, &ready_less, NULL));
    }

    if (cur != next)
        prev = switch_threads(cur, next);
    thread_schedule_tail(prev);
}

/* Donate current thread's priority to donee */
void donate_priority(struct thread *donee) {
    enum intr_level old_level;

    /* Check if donee is a valid thread */
    ASSERT(is_thread(donee));
    
    old_level = intr_disable();

    /* Set donees priority to current thread's priority */
    donee->donation_priority = thread_get_priority();

    list_sort(&ready_list, &ready_less, NULL);

    /* Make sure the list is ordered */
    if (!get_thread_mlfqs()) {
        ASSERT(list_sorted(&ready_list, &ready_less, NULL));
    }

    intr_set_level(old_level);
}
    

/*! Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire(&tid_lock);
    tid = next_tid++;
    lock_release(&tid_lock);

    return tid;
}

/*! Offset of `stack' member within `struct thread'.
    Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);

