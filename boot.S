.section .text
# Make _start visible to the linker.
.global _start

# Set the stack pointer to the top of the stack.
_start:
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
