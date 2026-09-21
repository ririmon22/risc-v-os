// Use C Linkage so the assembly code can call kernel_main without C++ name
// mangling.
void uart_putc(char c) {
  volatile unsigned char *uart =
      reinterpret_cast<volatile unsigned char *>(0x10000000);

  // Wait until ready.
  while ((uart[5] & 0x20) == 0) {
  }

  uart[0] = c;
}

void uart_puts(const char *text) {
  while (*text != '\0') {
    uart_putc(*text);
    ++text;
  }
}

void uart_put_hex(unsigned long value) {
  const char *digits = "0123456789abcdef";

  uart_puts("0x");

  for (int shift = 60; shift >= 0; shift -= 4) {
    uart_putc(digits[(value >> shift) & 0xf]);
  }
}

extern "C" void trap_handler() {
  unsigned long cause;
  asm volatile("csrr %0, mcause" : "=r"(cause));
  uart_puts("mcause = ");
  uart_put_hex(cause);
  uart_puts("\r\n");

  if (cause == 3) {
    uart_puts("Trap: breakpoint\r\n");
  } else {
    uart_puts("Trap: unknown\r\n");
  }

  for (;;) {
  }
}

extern "C" void kernel_main() {
  uart_puts("Hello!\r\n");

  asm volatile("ebreak");
  for (;;) {
  }
}
