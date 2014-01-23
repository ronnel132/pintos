#include "threads/loader.h"

#### Kernel loader.

#### This code should be stored in the first sector of a hard disk.
#### When the BIOS runs, it loads this code at physical address
#### 0x7c00-0x7e00 (512 bytes) and jumps to the beginning of it,
#### in real mode.  The loader loads the kernel into memory and jumps
#### to its entry point, which is the start function in start.S.
####
#### The BIOS passes in the drive that the loader was read from as
#### DL, with floppy drives numbered 0x00, 0x01, ... and hard drives
#### numbered 0x80, 0x81, ...  We want to support booting a kernel on
#### a different drive from the loader, so we don't take advantage of
#### this.

# Runs in real mode, which is a 16-bit segment.
	.code16

# Set up segment registers.
# Set stack to grow downward from 60 kB (after boot, the kernel
# continues to use this stack for its initial thread).

	sub %ax, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov $0xf000, %esp

# Configure serial port so we can report progress without connected VGA.
# See [IntrList] for details.
	sub %dx, %dx			# Serial port 0.
	mov $0xe3, %al			# 9600 bps, N-8-1.
					# AH is already 0 (Initialize Port).
	int $0x14			# Destroys AX.


# Set the hard drive number in dl to be HD No. - 1 (Since we immediately 
# increment.
mov $0x7F, %dl 

# read_hd is the main loop for iterating through hard drives. 
read_hd:
    # Incementing hard drive (or setting it to 0x80 for the first time
    inc %dl 

    # Check if we've reached the 26th drive; we then abort
    cmp $0x99, %dl
    je no_drive_found
	    
    # Load the MBR for the current HD

	# Set init_dap's register arguments

	# Set eax (the LBA starting sector index) to be 0 (for the MBR)
    mov $0x0, %eax

    # Set the number of sectors we are reading as 1 (only reading the MBR) 
    mov $0x01, %di
    
	# Call init_dap, which sets up the DAP, and calls int 13, and stores the
	# results in 0x2000:0x0000
    call init_dap

	# Set es to be 0x2000. We use this to refer to the newly loaded MBR (it 
	# was loaded into this location).    
    mov $0x2000, %cx
    mov %cx, %es

# Check if sector 0 ends with 0x55, 0xAA. Otherwise skip drive
    cmpb $0x55, %es:0x1FE
    jne read_hd

    cmpb $0xAA, %es:0x1FF
    jne read_hd

# Iterate through the partition table in the MBR

# bx is our iterator for reading the partition table entries. We set it to
# the offset of the first partition table entry - 0x10 (since we add 0x10
# in the beginning of the loop, and each partition table entry is 0x10 
# bytes).
    mov $0x1AE, %bx

# Loop through partitions in the partition table.
read_pt_entry:
    # If we finished checking partition 4 (when the offset bx is equal to the
    # offset of the last partition table entry), leave this hd.
    cmp $0x1EE, %bx
    je read_hd

    # Go to next partition table entry
    add $0x10, %bx

    # Check if the drive status of the current partition is bootable (0x80).
	# If not, jump to the next partition.
    cmpb $0x80, %es:(%bx)
    jne read_pt_entry

    # Check if the type of the current partition is a Pintos partition.
	# If not, jump to the next partition.
    cmpb $0x20, %es:4(%bx)
    jne read_pt_entry

# If we've reached this point, then the current partition is a bootable 
# Pintos partition.

	# Start setting register arguments for init_dap so we can load the 
	# kernel code.

	# Move the partition table entry (PTE) memory address at offset 8 (the 
	# LBA address of the starting sector) into eax.
    movl %es:8(%bx), %eax

	# Compare 0x400 (1024 in dec) to the number of sectors specified by the
	# PTE stored at offset 12. We store the minimum of these two values in
	# di (which init_dap stores in the DAP as the number of sectors to read).
	# Thus, we read up to 512 KiB, or less if the partition is smaller than 
	# that.
    cmpw $0x400, %es:12(%bx)
	
	# Jump to set_min_sector_read_1024 if the number of sectors in the 
	# partition is greater than 0x400.
    jg set_min_sector_read_1024

	# Otherwise jump to set_min_sector_read_pintos_size
    jmp set_min_sector_read_pintos_size

set_min_sector_read_1024:
    mov $0x400, %di
	
	# Jump to done and call init_dap
    jmp done
set_min_sector_read_pintos_size:
	# The number of sectors in this partition is less than 0x400, so move it 
	# into di.
    mov %es:12(%bx), %di

done:
	# Call init_dap to move the kernel partition to 0x20000
    call init_dap

	# We're good to go.
    jmp good_to_go

# drive_error is jumped to in init_dap after int 13 is called and CF=1, 
# signifying we have reached the last HD.
drive_error:
	call puts
	.string "Read error"
	hlt

no_drive_found:
	call puts
	.string "No drive found"
    hlt

good_to_go:
	call puts
	.string "Reading hd"

	mov $0x6161, %bx
	cmpb $0x80, %dl
	cmove %bx, %ax

	mov $0x6262, %bx
	cmpb $0x81, %dl
	cmove %bx, %ax

	mov $0x6363, %bx
	cmpb $0x82, %dl
	cmove %bx, %ax

	mov $0x6464, %bx
	cmpb $0x83, %dl
	cmove %bx, %ax

	call putc

	
#### Transfer control to the kernel that we loaded.  We read the start
#### address out of the ELF header (see [ELF1]) and convert it from a
#### 32-bit linear address into a 16:16 segment:offset address for
#### real mode, then jump to the converted address.  The 80x86 doesn't
#### have an instruction to jump to an absolute segment:offset kept in
#### registers, so in fact we store the address in a temporary memory
#### location, then jump indirectly through that location.  To save 4
#### bytes in the loader, we reuse 4 bytes of the loader's code for
#### this temporary pointer.

	mov $0x2000, %ax
	mov %ax, %es
	mov %es:0x18, %dx
	mov %dx, start
	movw $0x2000, start + 2
	ljmp *start

#### The following label is a placeholder for the preceding code to use
#### to set up for the jump to the Pintos kernel.  Of course, the start
#### label also points to the puts function; when the previous code runs,
#### it will clobber a few of the bytes at the start of the puts function,
#### but hey we're done bootloading, so we don't need it anymore.

start:

#### Print string subroutine.  To save space in the loader, this
#### subroutine takes its null-terminated string argument from the
#### code stream just after the call, and then returns to the byte
#### just after the terminating null.  This subroutine preserves all
#### general-purpose registers.

puts:	xchg %si, %ss:(%esp)
	push %ax
next_char:
	mov %cs:(%si), %al
	inc %si
	test %al, %al
	jz 1f
	call putc
	jmp next_char
1:	pop %ax
	xchg %si, %ss:(%esp)
	ret

#### Character output subroutine.  Prints the character in AL to the
#### VGA display and serial port 0, using BIOS services (see
#### [IntrList]).  Preserves all general-purpose registers.
####
#### If called upon to output a carriage return, this subroutine
#### automatically supplies the following line feed.

putc:	pusha

1:	sub %bh, %bh			# Page 0.
	mov $0x0e, %ah			# Teletype output service.
	int $0x10

	mov $0x01, %ah			# Serial port output service.
	sub %dx, %dx			# Serial port 0.
2:	int $0x14			# Destroys AH.
	test $0x80, %ah			# Output timed out?
	jz 3f
	movw $0x9090, 2b		# Turn "int $0x14" above into NOPs.

3:
	cmp $'\r', %al
	jne popa_ret
	mov $'\n', %al
	jmp 1b

popa_ret:
	popa
	ret				# Error code still in CF

# Helper function to create a data access packet and call int 13.
# We hard code the buffer pointer to be the location 0x2000:0x0000, since
# this is where the kernel code will end up eventually. We also have two 
# register arguments: eax, which stores the starting sector to read, and 
# di, which stores the number of sectors to read.

init_dap:
	# Save the registers onto the stack.
    pusha

    # First push the starting sector to read (in LBA).
	# We push 4 bytes of 0's first since the start sector is 8 bytes long.
    pushl $0x0
	# Push the register argument for start sector index.
    pushl %eax

    # Next push the register storing the location of the buffer. We hard
	# code this location to be 0x2000:0x0000. 
    push $0x2000 
    push $0x0

    # Push the number of sectors to read.
    push %di

    # Push the size of the DAP (always 0x10) and the unused byte both at
	# once (since we can't push bytes).
    push $0x0010

	# Set ds:si by moving the current stack pointer into si. (ds = ss).
    mov %sp, %si

    # Now call the interupt
    mov $0x42, %ah
    int $0x13

# Check if there was an error in accessing the hd by checking the CF flag
    jc drive_error

    # As it turns out, we're pushing exactly 16 bytes, which is
    # the total size of all registers. So we can save lines of code
    # by doing a popa.
    popa

	# Now pop the registers back.
    popa
    
    ret

#### Command-line arguments and their count.
#### This is written by the `pintos' utility and read by the kernel.
#### The loader itself does not do anything with the command line.
	.org LOADER_ARG_CNT - LOADER_BASE
	.fill LOADER_ARG_CNT_LEN, 1, 0

	.org LOADER_ARGS - LOADER_BASE
	.fill LOADER_ARGS_LEN, 1, 0

#### Partition table.
	.org LOADER_PARTS - LOADER_BASE
	.fill LOADER_PARTS_LEN, 1, 0

#### Boot-sector signature for BIOS inspection.
	.org LOADER_SIG - LOADER_BASE
	.word 0xaa55

