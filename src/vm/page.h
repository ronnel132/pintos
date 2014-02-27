#include <list.h>

struct vm_area_struct {
    /* Virtual memory start and end addresses. */
    void *vm_start;
    void *vm_end;
    unsigned long vm_flags; 

    /* Pointer to the file object of the mapped file, if any. */
    struct file *vm_file;

    /* Offset in the mapped file. */
    unsigned long vm_pgoff;

    list_elem elem;
};
