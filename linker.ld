ENTRY(_start)

SECTIONS
{
  . = 0x80000000;

  .text : {
    boot.o(.text)
    *(.text .text.*)
  }

  .rodata : {
    *(.rodata .rodata.*)
  }

  .data : {
    *(.data .data.*)
    *(.sdata .sdata.*)
  }

  .bss : {
    __bss_start = .;
    *(.bss .bss.*)
    *(.sbss .sbss.*)
    __bss_end = .;
  }
}
