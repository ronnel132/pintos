#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

/*! Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);

/*! Lock used by filesystem syscalls. */
extern struct lock filesys_lock;

/*! Registers handlers for interrupts that can be caused by user programs.

    In a real Unix-like OS, most of these interrupts would be passed along to
    the user process in the form of signals, as described in [SV-386] 3-24 and
    3-25, but we don't implement signals.  Instead, we'll make them simply kill
    the user process.

    Page faults are an exception.  Here they are treated the same way as other
    exceptions, but this will need to change to implement virtual memory.

    Refer to [IA32-v3a] section 5.15 "Exception and Interrupt Reference" for a
    description of each of these exceptions. */
void exception_init(void) {
    /* These exceptions can be raised explicitly by a user program,
       e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
       we set DPL==3, meaning that user programs are allowed to
       invoke them via these instructions. */
    intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
    intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
    intr_register_int(5, 3, INTR_ON, kill,
                      "#BR BOUND Range Exceeded Exception");

    /* These exceptions have DPL==0, preventing user processes from
       invoking them via the INT instruction.  They can still be
       caused indirectly, e.g. #DE can be caused by dividing by
       0.  */
    intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
    intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
    intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
    intr_register_int(7, 0, INTR_ON, kill,
                      "#NM Device Not Available Exception");
    intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
    intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
    intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
    intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
    intr_register_int(19, 0, INTR_ON, kill,
                      "#XF SIMD Floating-Point Exception");

    /* Most exceptions can be handled with interrupts turned on.
       We need to disable interrupts for page faults because the
       fault address is stored in CR2 and needs to be preserved. */
    intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/*! Prints exception statistics. */
void exception_print_stats(void) {
    printf("Exception: %lld page faults\n", page_fault_cnt);
}

/*! Handler for an exception (probably) caused by a user process. */
static void kill(struct intr_frame *f) {
    /* This interrupt is one (probably) caused by a user process.
       For example, the process might have tried to access unmapped
       virtual memory (a page fault).  For now, we simply kill the
       user process.  Later, we'll want to handle page faults in
       the kernel.  Real Unix-like operating systems pass most
       exceptions back to the process via signals, but we don't
       implement them. */
     
    /* The interrupt frame's code segment value tells us where the
       exception originated. */
    switch (f->cs) {
    case SEL_UCSEG:
        /* User's code segment, so it's a user exception, as we
           expected.  Kill the user process.  */
        printf("%s: dying due to interrupt %#04x (%s).\n",
               thread_name(), f->vec_no, intr_name(f->vec_no));
        intr_dump_frame(f);
//         thread_exit(); 
        exit(-1);

    case SEL_KCSEG:
        /* Kernel's code segment, which indicates a kernel bug.
           Kernel code shouldn't throw exceptions.  (Page faults
           may cause kernel exceptions--but they shouldn't arrive
           here.)  Panic the kernel to make the point.  */
        intr_dump_frame(f);
        PANIC("Kernel bug - unexpected interrupt in kernel"); 

    default:
        /* Some other code segment?  Shouldn't happen.  Panic the
           kernel. */
        printf("Interrupt %#04x (%s) in unknown segment %04x\n",
               f->vec_no, intr_name(f->vec_no), f->cs);
        thread_exit();
    }
}

/*! Page fault handler.  This is a skeleton that must be filled in
    to implement virtual memory.  Some solutions to project 2 may
    also require modifying this code.

    At entry, the address that faulted is in CR2 (Control Register
    2) and information about the fault, formatted as described in
    the PF_* macros in exception.h, is in F's error_code member.  The
    example code here shows how to parse that information.  You
    can find more information about both of these in the
    description of "Interrupt 14--Page Fault Exception (#PF)" in
    [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void page_fault(struct intr_frame *f) {
    bool not_present;  /* True: not-present page, false: writing r/o page. */
    bool write;        /* True: access was write, false: access was read. */
    bool user;         /* True: access by user, false: access by kernel. */
    bool found_valid;  
    off_t bytes_read;
    void *fault_addr;  /* Fault address. */
    void *new_page;   /* New page that's being allocated */
    void *zero_start;
    struct thread *t = thread_current();
    struct list_elem *e;
    struct vm_area_struct *vma;
    void *esp; /* Esp of faulting thread */
    bool fs_lock = false;

    /* Obtain faulting address, the virtual address that was accessed to cause
       the fault.  It may point to code or to data.  It is not necessarily the
       address of the instruction that caused the fault (that's f->eip).
       See [IA32-v2a] "MOV--Move to/from Control Registers" and
       [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception (#PF)". */
    asm ("movl %%cr2, %0" : "=r" (fault_addr));

//     printf("pagefault_start\n");
//     printf("%d\n", f->esp);


    /* Turn interrupts back on (they were only off so that we could
       be assured of reading CR2 before it changed). */
    intr_enable();

    /* Count page faults. */
    page_fault_cnt++;

    /* Determine cause. */
    not_present = (f->error_code & PF_P) == 0;
    write = (f->error_code & PF_W) != 0;
    user = (f->error_code & PF_U) != 0;

#ifdef VM
    /* If we pagefaulted in kernel code, let's  assume we were coming from
     * a syscall, hence we have a valid esp for the thread (in user context).
     * If not this will freak out, but there's not much else we could do here
     * anyway, so it's OK
     */
//     if (!user) {
//         printf("page fault at %p: %s error %s page in %s context.\n",
//            fault_addr,
//            not_present ? "not present" : "rights violation",
//            write ? "writing" : "reading",
//            user ? "user" : "kernel");
//         kill(f);
//     }

    
    if (!user) {
        esp = t->esp;
    }
    else {
        esp = f->esp;
    }

    if (not_present) {
        /* Iterate through the current thread's supplemental page table to 
           find if the faulting address is valid. */

        found_valid = false;
        for (e = list_begin(&t->spt); e != list_end(&t->spt);
             e = list_next(e)) {
             vma = list_entry(e, struct vm_area_struct, elem);
             if (fault_addr >= vma->vm_start && fault_addr <= vma->vm_end) {
                found_valid = true;   
                break;
             }
        }
        if (found_valid) {
            new_page = palloc_get_page(PAL_USER); 
            if (new_page == NULL) {
                new_page = frame_evict();
            }
            if (vma->pg_type == FILE_SYS) {
                /* Read the file into the kernel page. If we do not read the
                   PGSIZE bytes, then zero out the rest of the page. */
                /* Seek to the correct offset. */

                /* Checking for lock holder should be atomic */
                intr_disable();
                if (!lock_held_by_current_thread(&filesys_lock)) {
                    fs_lock = true;
                    lock_acquire(&filesys_lock);
                }

                if (!lock_held_by_current_thread(&filesys_lock)) {
                    lock_acquire(&filesys_lock);
                    fs_lock = true;
                }
                intr_enable();

                file_seek(vma->vm_file, vma->ofs);

                /* Read from the file. */
                bytes_read = file_read(vma->vm_file, new_page,
                                      (off_t) vma->pg_read_bytes);

                if (fs_lock) {
                    lock_release(&filesys_lock);
                }

                ASSERT(bytes_read == vma->pg_read_bytes);
                memset(new_page + bytes_read, 0, PGSIZE - bytes_read);
            }
            else if (vma->pg_type == ZERO) {
                /* Zero out the page. */
                memset(new_page, 0, PGSIZE);
            }
            else if (vma->pg_type == SWAP) {
                ASSERT(vma->swap_ind != NULL);
                /* Read in from swap into the new page. */
                swap_remove(vma->swap_ind, new_page);
                vma->swap_ind = NULL;
                vma->pg_type = PMEM;
            }

            if (!pagedir_set_page(t->pagedir, pg_round_down(fault_addr),
                             new_page, vma->writable)) {
                kill(f);
            }
            /* Record the new kpage in the vm_area_struct. */
            vma->kpage = new_page;
            /* Add the new page-frame mapping to the frame table. */
            frame_add(t, pg_round_down(fault_addr), new_page);
        }
        else {
            if ((fault_addr == esp - 4) || (fault_addr == esp - 32)) {
                /* Check for stack overflow */
                if (fault_addr < STACK_MIN) {
                    exit(-1);
                }

                /* If we're here, let's give this process another page */
                new_page = palloc_get_page(PAL_ZERO | PAL_USER);
                if (new_page == NULL) {
                    new_page = frame_evict();
                }
                pagedir_set_page(t->pagedir, pg_round_down(fault_addr),
                                 new_page, 1);

                /* Record the new stack page in the supplemental page table and 
                   the frame table. */
                vma = (struct vm_area_struct *) 
                      malloc(sizeof(struct vm_area_struct));
                vma->vm_start = pg_round_down(fault_addr);
                vma->vm_end = pg_round_down(fault_addr) + PGSIZE 
                              - sizeof(uint8_t);
                vma->kpage = new_page;
                vma->pg_read_bytes = NULL;
                vma->writable = true;
                vma->pinned = false;
                vma->vm_file = NULL;
                vma->ofs = NULL;
                vma->swap_ind = NULL;
                vma->pg_type = PMEM;

                spt_add(thread_current(), vma);

                frame_add(t, pg_round_down(fault_addr), new_page);
            }
            else if (fault_addr >= esp) {
                new_page = palloc_get_page(PAL_ZERO | PAL_USER);
                if (new_page == NULL) {
                    new_page = frame_evict();
                }
                pagedir_set_page(t->pagedir, pg_round_down(fault_addr),
                                 new_page, 1);
                                 
                /* Record the new stack page in the supplemental page table and 
                   the frame table. */
                vma = (struct vm_area_struct *) 
                      malloc(sizeof(struct vm_area_struct));
                vma->vm_start = pg_round_down(fault_addr);
                vma->vm_end = pg_round_down(fault_addr) + PGSIZE 
                              - sizeof(uint8_t);
                vma->kpage = new_page;
                vma->pg_read_bytes = NULL;
                vma->writable = true;
                vma->pinned = false;
                vma->vm_file = NULL;
                vma->ofs = NULL;
                vma->swap_ind = NULL;
                vma->pg_type = PMEM;

                spt_add(thread_current(), vma);

                frame_add(t, pg_round_down(fault_addr), new_page);
            }
            
            /* Else is probably an invalid access */
            else {
//                 printf("Page fault at %p: %s error %s page in %s context.\n",
//                        fault_addr,
//                        not_present ? "not present" : "rights violation",
//                        write ? "writing" : "reading",
//                        user ? "user" : "kernel");
//                 kill(f);
                exit(-1);
            }
        }
    }
    /* Rights violation */
    else {
//         printf("Page fault at %p: %s error %s page in %s context.\n",
//                fault_addr,
//                not_present ? "not present" : "rights violation",
//                write ? "writing" : "reading",
//                user ? "user" : "kernel");
//         kill(f);
        exit(-1);
    }

#else
    /* To implement virtual memory, delete the rest of the function
       body, and replace it with code that brings in the page to
       which fault_addr refers. */
    printf("Page fault at %p: %s error %s page in %s context.\n",
           fault_addr,
           not_present ? "not present" : "rights violation",
           write ? "writing" : "reading",
           user ? "user" : "kernel");
    kill(f);

#endif
//     printf("pagefault_end\n");
}

