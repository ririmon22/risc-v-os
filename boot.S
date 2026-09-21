.section .text
# Make _start visible to the linker.
.global _start

# Set the stack pointer to the top of the stack.
_start:
    la sp, stack_top
# Call the kernel_main
    call kernel_main

.section .bss
# Reserve 4 KiB for the stack 
# The Stack grows from high to low address.
stack_bottom:
    .space 4096
stack_top:
