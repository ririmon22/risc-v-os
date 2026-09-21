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
    *(.bss .bss.*)
    *(.sbss .sbss.*)
  }
}
