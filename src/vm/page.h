#include <list.h>

/* What type of pages are stored in this vm_area? They can be in the file 
   system, in swap, or an all zero page. */
enum pg_type_flags {
    FILESYS,
    SWAP,
    ZERO
};

struct vm_area_struct {
    /* Virtual memory start and end addresses. */
    void *vm_start;
    void *vm_end;
    unsigned long vm_flags; 

    /* Pointer to the file object of the mapped file, if any. */
    struct file *vm_file;

    /* Offset in the mapped file. */
    unsigned long vm_pgoff;

    enum pg_type_flags pg_type;

    list_elem elem;
};
