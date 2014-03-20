/*! \file synch.c
 *
 * Implementation of various thread synchronization primitives.
 */

/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"


/* Declare pri_donation_list struct from the header file definition */
extern struct list pri_donation_list;

/*! Initializes semaphore SEMA to VALUE.  A semaphore is a
    nonnegative integer along with two atomic operators for
    manipulating it:

    - down or "P": wait for the value to become positive, then
      decrement it.

    - up or "V": increment the value (and wake up one waiting
      thread, if any). */
void sema_init(struct semaphore *sema, unsigned value) {
    ASSERT(sema != NULL);

    sema->value = value;
    list_init(&sema->waiters);
}

/*! Down or "P" operation on a semaphore.  Waits for SEMA's value
    to become positive and then atomically decrements it.

    This function may sleep, so it must not be called within an
    interrupt handler.  This function may be called with
    interrupts disabled, but if it sleeps then the next scheduled
    thread will probably turn interrupts back on. */
void sema_down(struct semaphore *sema) {
    enum intr_level old_level;

    ASSERT(sema != NULL);
    ASSERT(!intr_context());

    old_level = intr_disable();
    while (sema->value == 0) {
        /* Insert thread ordered by priority */
        list_insert_ordered(&sema->waiters, &thread_current()->elem, 
            &ready_less, NULL);
        thread_block();
    }
    sema->value--;
    intr_set_level(old_level);
}

/*! Down or "P" operation on a semaphore, but only if the
    semaphore is not already 0.  Returns true if the semaphore is
    decremented, false otherwise.

    This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema) {
    enum intr_level old_level;
    bool success;

    ASSERT(sema != NULL);

    old_level = intr_disable();
    if (sema->value > 0) {
        sema->value--;
        success = true; 
    }
    else {
      success = false;
    }
    intr_set_level(old_level);

    return success;
}

/*! Up or "V" operation on a semaphore.  Increments SEMA's value
    and wakes up one thread of those waiting for SEMA, if any.

    This function may be called from an interrupt handler. */
void sema_up(struct semaphore *sema) {
    enum intr_level old_level;
    struct list_elem *e;
    struct thread *max_waiter, *t;
    int max_pri = -1;

    ASSERT(sema != NULL);

    old_level = intr_disable();
    if (!list_empty(&sema->waiters)) {
        /* Find thread with maximum priority that's waiting on this
         * semaphore
         */
        for (e = list_begin(&sema->waiters); e != list_end(&sema->waiters);
             e = list_next(e)) {
            t = list_entry(e, struct thread, elem);

            /* Make sure priority makes sense */
            ASSERT ((effective_priority(t) >= PRI_MIN) && 
                (effective_priority(t) <= PRI_MAX));

            if (effective_priority(t) > max_pri) {
                max_pri = effective_priority(t);
                max_waiter = t;
            }	
        }	
        ASSERT(max_pri != -1);

        list_remove(&max_waiter->elem);

        sema->value++;

        /* Unblock that thread, as it's good to go */
        thread_unblock(max_waiter);

        /* If the waiter that just got unblocked has higher priority */
        if (max_pri >= thread_get_priority()) {

            /* Context switch to highe rpriority thread */
            if (!intr_context()) {
                thread_yield();
            }
            else {
                intr_yield_on_return();
            }
        }
    }
    else {
        /* Increment semaphore for the other case too */
        sema->value++;
    }
    intr_set_level(old_level);
}

static void sema_test_helper(void *sema_);

/*! Self-test for semaphores that makes control "ping-pong"
    between a pair of threads.  Insert calls to printf() to see
    what's going on. */
void sema_self_test(void) {
    struct semaphore sema[2];
    int i;

    printf("Testing semaphores...");
    sema_init(&sema[0], 0);
    sema_init(&sema[1], 0);
    thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
    for (i = 0; i < 10; i++) {
        sema_up(&sema[0]);
        sema_down(&sema[1]);
    }
    printf ("done.\n");
}

/*! Thread function used by sema_self_test(). */
static void sema_test_helper(void *sema_) {
    struct semaphore *sema = sema_;
    int i;

    for (i = 0; i < 10; i++) {
        sema_down(&sema[0]);
        sema_up(&sema[1]);
    }
}

/*! Initializes LOCK.  A lock can be held by at most a single
    thread at any given time.  Our locks are not "recursive", that
    is, it is an error for the thread currently holding a lock to
    try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock *lock) {
    ASSERT(lock != NULL);

    lock->holder = NULL;
    sema_init(&lock->semaphore, 1);
}

/*! Acquires LOCK, sleeping until it becomes available if
    necessary.  The lock must not already be held by the current
    thread.

    This function may sleep, so it must not be called within an
    interrupt handler.  This function may be called with
    interrupts disabled, but interrupts will be turned back on if
    we need to sleep. */
void lock_acquire(struct lock *lock) {
	struct priority_donation_state *pd_state;
	struct thread *next_donee;
    enum intr_level old_level;
    ASSERT(lock != NULL);
//     ASSERT(!intr_context());
    if (intr_context()) {
        ASSERT(0);
    }
//     ASSERT(!lock_held_by_current_thread(lock));
    if(lock_held_by_current_thread(lock)) {
        ASSERT(0);
    }


    /* Only do this if we aren't using MLFQS option. */
    if (!get_thread_mlfqs()) {
        old_level = intr_disable();
        /* If this lock is being held by another thread, and
         * that threads priority is less than current thread's priority,
         * donate priority to that thread
         */
        if ((lock->holder != NULL) && 
            (effective_priority(lock->holder) < thread_get_priority())) {
            pd_state = palloc_get_page(PAL_ZERO);
            if (pd_state == NULL) {
                thread_yield();
            }	
            thread_current()->donee = lock->holder;
            pd_state->donee = lock->holder;
            pd_state->lock_desired = lock;
            pd_state->donor = thread_current();
            list_push_front(&pri_donation_list, &pd_state->elem);	
            donate_priority(lock->holder);
            /* Check if the donee is blocked. If so, then set priorities down
               the chain the last donee manually. */
            if (lock->holder->status == THREAD_BLOCKED) {
                next_donee = lock->holder->donee;
                /* Iterate through the blocked threaad's donation chain, and set
                   the priorities of these donees along this chain manually. */
                while (next_donee != NULL) {
                    donate_priority(next_donee);
                    next_donee = next_donee->donee;
                }
            }
        }
        intr_set_level(old_level);
    }

        

    sema_down(&lock->semaphore);
    lock->holder = thread_current();
}

/*! Tries to acquires LOCK and returns true if successful or false
    on failure.  The lock must not already be held by the current
    thread.

    This function will not sleep, so it may be called within an
    interrupt handler. */
bool lock_try_acquire(struct lock *lock) {
    bool success;

    ASSERT(lock != NULL);
    ASSERT(!lock_held_by_current_thread(lock));

    success = sema_try_down(&lock->semaphore);
    if (success)
      lock->holder = thread_current();

    return success;
}

/*! Releases LOCK, which must be owned by the current thread.

    An interrupt handler cannot acquire a lock, so it does not
    make sense to try to release a lock within an interrupt
    handler. */
void lock_release(struct lock *lock) {
    int largest_donated_pri;
	struct list_elem *e;
	struct priority_donation_state *cur_state;
	largest_donated_pri = -1;
    enum intr_level old_level;

    ASSERT(!intr_context());
    ASSERT(lock != NULL);
//     ASSERT(lock_held_by_current_thread(lock));
    if(!lock_held_by_current_thread(lock)) {
        ASSERT(0);
    }
    
    /* If MLFQS option is not enabled, use priority donation. */
    if (!get_thread_mlfqs()) {
        old_level = intr_disable();

        /* If the donation list isn't empty */
        if (!list_empty(&pri_donation_list)) {
            e = list_begin(&pri_donation_list);
            while (e != list_end(&pri_donation_list)) {
                cur_state = list_entry(e, struct priority_donation_state, elem);
                if (cur_state->lock_desired == lock && 
                    cur_state->donee == thread_current()) {

                    /* Jump to next one, then delete current, as this
                     * lock is being released
                     */
                    e = list_next(e);
                    list_remove(&cur_state->elem);
                    palloc_free_page(cur_state);
                }
                else {
                    e = list_next(e);
                }
                
            }

            /* Set the donated priority of the current thread to the
             * largest donated priority of all its current donors
             */
            for (e = list_begin(&pri_donation_list); 
                 e != list_end(&pri_donation_list); e = list_next(e)) {
                cur_state = list_entry(e, struct priority_donation_state, elem);
                if ((effective_priority(cur_state->donor) > largest_donated_pri) && 
                        (cur_state->donee == thread_current())) {
                    largest_donated_pri = effective_priority(cur_state->donor);
                }
            }

            thread_current()->donation_priority = largest_donated_pri;
        }
        intr_set_level(old_level);
    }

    lock->holder = NULL;
	/* If this thread donated to another thread, then its donee will be reset
	   back to NULL (default value). */
	thread_current()->donee = NULL;
    sema_up(&lock->semaphore);
}

/*! Returns true if the current thread holds LOCK, false
    otherwise.  (Note that testing whether some other thread holds
    a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock) {
    ASSERT(lock != NULL);

    return lock->holder == thread_current();
}

/*! One semaphore in a list. */
struct semaphore_elem {
    struct list_elem elem;              /*!< List element. */
    struct semaphore semaphore;         /*!< This semaphore. */
};

/*! Initializes condition variable COND.  A condition variable
    allows one piece of code to signal a condition and cooperating
    code to receive the signal and act upon it. */
void cond_init(struct condition *cond) {
    ASSERT(cond != NULL);

    list_init(&cond->waiters);
}

/*! Atomically releases LOCK and waits for COND to be signaled by
    some other piece of code.  After COND is signaled, LOCK is
    reacquired before returning.  LOCK must be held before calling
    this function.

    The monitor implemented by this function is "Mesa" style, not
    "Hoare" style, that is, sending and receiving a signal are not
    an atomic operation.  Thus, typically the caller must recheck
    the condition after the wait completes and, if necessary, wait
    again.

    A given condition variable is associated with only a single
    lock, but one lock may be associated with any number of
    condition variables.  That is, there is a one-to-many mapping
    from locks to condition variables.

    This function may sleep, so it must not be called within an
    interrupt handler.  This function may be called with
    interrupts disabled, but interrupts will be turned back on if
    we need to sleep. */
void cond_wait(struct condition *cond, struct lock *lock) {
    struct semaphore_elem waiter;
    enum intr_level old_level;

    old_level = intr_disable();

    ASSERT(cond != NULL);
    ASSERT(lock != NULL);
    ASSERT(!intr_context());
    ASSERT(lock_held_by_current_thread(lock));
  
    sema_init(&waiter.semaphore, 0);
    list_push_back(&cond->waiters, &waiter.elem);
    lock_release(lock);
    sema_down(&waiter.semaphore);
    lock_acquire(lock);

    intr_set_level(old_level);
}

/*! If any threads are waiting on COND (protected by LOCK), then
    this function signals one of them to wake up from its wait.
    LOCK must be held before calling this function.

    An interrupt handler cannot acquire a lock, so it does not
    make sense to try to signal a condition variable within an
    interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED) {
	struct list_elem *e;
	struct semaphore *s;
	struct thread *t;
    struct semaphore_elem *max_sem_elem;
	int max_pri = -1;
	struct semaphore *max_sem;
    enum intr_level old_level;

    ASSERT(cond != NULL);
    ASSERT(lock != NULL);
    ASSERT(!intr_context ());
    ASSERT(lock_held_by_current_thread (lock));

    old_level = intr_disable();

    /* Loop through all waiters and wake up the waiter with the
     * highest priority. We can't guarantee that this list is sorted
     * as it may have changed in the meantime
     */
    if (!list_empty(&cond->waiters)) {
        for (e = list_begin(&cond->waiters); e != list_end(&cond->waiters);
             e = list_next(e)) {
            s = &list_entry(e, struct semaphore_elem, elem)->semaphore;
            if (!list_empty(&s->waiters)) {
                t = list_entry(list_begin(&s->waiters), struct thread, elem);
                if (effective_priority(t) > max_pri) {
                    max_pri = effective_priority(t);
                    max_sem = s;
                    max_sem_elem = list_entry(e, struct semaphore_elem, elem);
                }	
            }
        }	
        ASSERT(max_pri != -1);
        /* Signal to the highest priority thread. */
        list_remove(max_sem_elem);
        sema_up(max_sem);
    }

    intr_set_level(old_level);
}

/*! Wakes up all threads, if any, waiting on COND (protected by
    LOCK).  LOCK must be held before calling this function.

    An interrupt handler cannot acquire a lock, so it does not
    make sense to try to signal a condition variable within an
    interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock) {
    ASSERT(cond != NULL);
    ASSERT(lock != NULL);

    while (!list_empty(&cond->waiters))
        cond_signal(cond, lock);
}

