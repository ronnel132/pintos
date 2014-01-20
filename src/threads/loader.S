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
    mov $0x42, %ah # Setting extended r/w sector mode
    inc %dl # Go to next (or first, for the first time) hard drive

# Set the segment and offset of the buffer for the MBR.
# Move segment 0x00 into ax, and offset 0x10 (after the DAP) 
# into bx.

    mov $0x00, %ax
    mov $0x10, %bx
    
# Set the offset pointer to the DAP to be 0.
    sub %si, %si


    # Initialize cx to 0 (this will keep track of ds:si
    sub %cx, %cx

    mov $0x10, (%cx)

    mov $0x00, 1(%cx) 

    # Set the low byte of the number of sectors we are reading as 1
    mov $0x01, 2(%cx)

    # Set the high byte of the number of sectors were are reading to 0
    mov $0x00, 3(%cx) 

    # Move the segment and offset value of the MBR buffer into bytes 4 - 7
    mov %al, 4(%cx)
    mov %ah, 5(%cx)
    mov %bl, 6(%cx)
    mov %bh, 7(%cx)


#    mov $0x10, %ds:(%si) # Set the size in the DAP struct to be 16
#    mov $0x00, 1(%ds:%si) 
#    # Set the low byte of the number of sectors we are reading as 1
#    mov $0x01, 2(%ds:%si)
#    # Set the high byte of the number of sectors were are reading to 0
#    mov $0x00, 3(%ds:%si) # Set the 
#    # Move the segment and offset value of the MBR buffer into bytes 4 - 7
#    mov %al, 4(%ds:%si)
#    mov %ah, 5(%ds:%si)
#    mov %bl, 6(%ds:%si)
#    mov %bh, 7(%ds:%si)

# Set the starting index of the sectors to read to be 0
    mov $0x08, %cx
set_index_bytes:
    mov $0x00, (%ds:%si, %cx,)
    inc %cx
    cmp %cx, $0x10
    jne set_index_bytes

#   Now call the interupt
    int $0x13

# Check if sector 0 ends with 0x55, 0xAA. Otherwise skip drive
    cmp (%ax:%bx, 0x1FE,), 0x55
    jne read_hd

    cmp (%ax:%bx, 0x1FF,), 0xAA
    jne read_hd

# Iterate through the partition table in the MBR
# Move the starting hex-off of the partition table into cx
    mov $0x1AE, %cx
read_pt_entry:
    # If we finished checking partition 4, leave this hd
    cmp %cx, 0x1EE
    je read_hd

    # Go to next partition
    add $0x10, %cx

    # Check if the drive status of the current partition is bootable (0x80)
    cmp (%ax:%bx, %cx,), $0x80
    jne read_pt_entry

    # Check if the type of the current partition is a Pintos partition
    cmp 4(%ax:%bx, %cx,), $0x20
    jne read_pt_entry

# If we've reached this point, then the current partition is a bootable 
# Pintos partition.

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

