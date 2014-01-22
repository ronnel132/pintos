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

	call puts
	.string "PiLo"

#### TODO:  ITERATE THROUGH ALL HARD DISKS IN THE SYSTEM, LOOKING FOR A
####        BOOTABLE PINTOS PARTITION TO LOAD AND START.
####
####        Note that there are only ~300 bytes available for the entire
####        Pintos bootloader, as Pintos commands are stored in the
####        region immediately before the partition table in the sector.
####        Sooo... be concise!
####
####        If you want to create helper routines, there is a TODO toward
####        the end of this file that shows where they should go.

mov $0x7F, %dl # Hard drive number -1 (since we're immediately incrementing)

read_hd:
    call puts
    .string "read_hd"

    inc %dl # Go to next (or first, for the first time) hard drive
    
    # Load the MBR for the current HD
    # Set init_dap's arguments
    mov $0x0, %ax
    mov $0x0, %bx
    mov %ds, %cx
    mov $0x1, %di
    
    call init_dap
    
    call puts
    .string "called init_dap"

#   Now call the interupt
    mov $0x42, %ah
    int $0x13

# Check if there was an error in accessing the hd by checking the CF flag
    jb drive_error
    
# Check if sector 0 ends with 0x55, 0xAA. Otherwise skip drive
    cmpb $0x55, %ds:0x1FE
    jne read_hd

    cmpb $0xAA, %ds:0x1FF
    jne read_hd

    call puts
    .string "HERE"

# Iterate through the partition table in the MBR
# Move the starting hex-off of the partition table into cx
    mov $0x1AE, %bx
read_pt_entry:
    # If we finished checking partition 4, leave this hd
    cmp $0x1EE, %bx
    je read_hd

    # Go to next partition table entry
    add $0x10, %bx

    # Check if the drive status of the current partition is bootable (0x80)
    cmpb $0x80, %ds:(%bx)
    jne read_pt_entry

    # Check if the type of the current partition is a Pintos partition
    cmp $0x20, %ds:4(%bx)
    jne read_pt_entry

# If we've reached this point, then the current partition is a bootable 
# Pintos partition.

drive_error:

	call puts
	.string "\r\rTODO:  IMPLEMENT THE BOOTLOADER\r"

	# Stop the processor.
	hlt

#### TODO:  LOADING CODE ENDS HERE!

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


#### TODO:  YOU CAN PUT HELPER FUNCTIONS HERE IF YOU WISH.

# Helper function to create a data access packet. Buffer segment will be
# specified in %cx, number of sectors to read in %di, and the starting 
# sector to read in %ax:%bx. The DAP will then be stored in %ds:%si. 
init_dap:
   # push %ebp
    #mov %ebp, %esp
    call puts
    .string "init_dap"
    pusha
    # First push the starting sector to read (in LBA)
    # Push the sector segment first
    push $0x0
    push %ax
    push $0x0
    push %bx

    # Next push the register storing the location of the buffer. We assume
    # the offset will be 0 for this location.
    push %cx
    push $0x0

    # Push the number of sectors to read
    push %di

    # Unused byte
    # Bello: The two pushb's generate a compiler error, so for some reason
    # we can't push a byte on the stack. Hence I changed to push
    # and added a 0 for the first command.

    # Push the size of the DAP (always 0x10)
    push $0x10

    # Set ds:si
    mov %sp, %si
    call puts
    .string "HERE2"

    # As it turns out, we're pushing exactly 16 bytes, which is
    # the total size of all registers. So we can save lines of code
    # by doing a popa.
    popa
    popa
    
    # Clean up the stack frame
    #mov %ebp, %esp
    #pop %ebp
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

