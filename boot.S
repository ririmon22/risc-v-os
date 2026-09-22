.section .text
# Make _start visible to the linker.
.global _start

# Set the stack pointer to the top of the stack.
_start:
    # Clear Bss.
    la t0, __bss_start
    la t1, __bss_end

clear_bss:
    bgeu t0, t1, clear_bss_done
    sb zero, 0(t0)
    addi t0, t0, 1
    j clear_bss

clear_bss_done:
    # Set the stack pointer.
    la sp, stack_top

    # Set the trap entry.
    la t0, trap_entry
    csrw mtvec, t0
# Call the kernel_main
    call kernel_main

halt:
    j halt

.balign 4
trap_entry:
    call trap_handler
    j halt

.section .bss
# Reserve 4 KiB for the stack 
# The Stack grows from high to low address.
.balign 16
stack_bottom:
    .space 4096
stack_top:
