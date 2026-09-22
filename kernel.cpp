// Use C Linkage so the assembly code can call kernel_main without C++ name
#include <stdint.h>

struct VirtqDesc {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
};

static_assert(sizeof(VirtqDesc) == 16, "VirtqDesc must be 16 bytes");
constexpr unsigned int QUEUE_SIZE = 8;

alignas(16) VirtqDesc rx_desc[QUEUE_SIZE] = {};
alignas(16) VirtqDesc tx_desc[QUEUE_SIZE] = {};

struct VirtqAvail {
  uint16_t flags;
  uint16_t idx;
  uint16_t ring[QUEUE_SIZE];
};

alignas(2) VirtqAvail rx_avail = {};
alignas(2) VirtqAvail tx_avail = {};

struct VirtqUsedElem {
  uint32_t id;
  uint32_t len;
};

struct VirtqUsed {
  uint16_t flags;
  uint16_t idx;
  VirtqUsedElem ring[QUEUE_SIZE];
};

alignas(4) volatile VirtqUsed rx_used = {};
alignas(4) volatile VirtqUsed tx_used = {};

static_assert(sizeof(VirtqUsedElem) == 8, "VirtqUsedElem must be 8 bytes");
static_assert(sizeof(VirtqUsed) == 68, "VirtqUsed must be 68 bytes");

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
  unsigned long epc;
  asm volatile("csrr %0, mepc" : "=r"(epc));
  asm volatile("csrr %0, mcause" : "=r"(cause));

  uart_puts("mcause = ");
  uart_put_hex(cause);
  uart_puts("\r\n");

  uart_puts("mepc = ");
  uart_put_hex(epc);
  uart_puts("\r\n");

  if (cause == 3) {
    uart_puts("Trap: breakpoint\r\n");
  } else {
    uart_puts("Trap: unknown\r\n");
  }

  for (;;) {
  }
}

unsigned long find_virtio_net() {
  for (unsigned long slot = 0; slot < 8; ++slot) {
    unsigned long base = 0x10001000 + slot * 0x1000;
    volatile unsigned int *regs =
        reinterpret_cast<volatile unsigned int *>(base);

    if (regs[0] == 0x74726976 && regs[1] == 2 && regs[2] == 1) {
      return base;
    }
  }
  return 0;
}

bool setup_virtqueue(volatile unsigned int *regs, unsigned int queue,
                     VirtqDesc *desc, VirtqAvail *avail,
                     volatile VirtqUsed *used) {
  regs[0x30 / 4] = queue;
  if (regs[0x44 / 4] != 0) {
    return false;
  }
  if (regs[0x34 / 4] < QUEUE_SIZE) {
    return false;
  }

  regs[0x38 / 4] = QUEUE_SIZE;

  uint64_t desc_addr = reinterpret_cast<uintptr_t>(desc);
  uint64_t avail_addr = reinterpret_cast<uintptr_t>(avail);
  uint64_t used_addr = reinterpret_cast<uintptr_t>(used);

  // Descriptor table
  regs[0x80 / 4] = static_cast<uint32_t>(desc_addr);
  regs[0x84 / 4] = static_cast<uint32_t>(desc_addr >> 32);

  // Available ring
  regs[0x90 / 4] = static_cast<uint32_t>(avail_addr);
  regs[0x94 / 4] = static_cast<uint32_t>(avail_addr >> 32);

  // Used ring.
  regs[0xa0 / 4] = static_cast<uint32_t>(used_addr);
  regs[0xa4 / 4] = static_cast<uint32_t>(used_addr >> 32);

  // Publish the queue.
  asm volatile("fence iorw, iorw" ::: "memory");
  regs[0x44 / 4] = 1;

  return regs[0x44 / 4] == 1;
}

void virtio_net_begin(unsigned long base) {
  volatile unsigned int *regs = reinterpret_cast<volatile unsigned int *>(base);

  // Reset the device.
  regs[0x70 / 4] = 0;
  while (regs[0x70 / 4] != 0) {
  }

  // ACKNOWLEDGE
  regs[0x70 / 4] = 1;

  // ACKNOWLEDGE | DRIVER
  regs[0x70 / 4] = 1 | 2;

  uart_puts("virtio status = ");
  uart_put_hex(regs[0x70 / 4]);
  uart_puts("\r\n");

  regs[0x14 / 4] = 0;
  unsigned int features_low = regs[0x10 / 4];

  regs[0x14 / 4] = 1;
  unsigned int features_high = regs[0x10 / 4];

  uart_puts("features low = ");
  uart_put_hex(features_low);
  uart_puts("\r\n");

  uart_puts("featurtes high = ");
  uart_put_hex(features_high);
  uart_puts("\r\n");

  if ((features_low & 0x20) == 0 || (features_high & 0x01) == 0) {
    uart_puts("Required features missin\r\n");
    regs[0x70 / 4] = regs[0x70 / 4] | 128;
    return;
  }

  // Select MAC and VERSION_1.
  regs[0x24 / 4] = 0;
  regs[0x20 / 4] = 0x20;

  regs[0x24 / 4] = 1;
  regs[0x20 / 4] = 0x01;

  // Set FEATURES_OK.
  regs[0x70 / 4] = 1 | 2 | 8;

  unsigned int status = regs[0x70 / 4];
  if ((status & 8) == 0) {
    uart_puts("Features rejected\r\n");
    regs[0x70 / 4] = status | 128;
    return;
  }
  uart_puts("Features accepted\r\n");

  volatile unsigned char *config =
      reinterpret_cast<volatile unsigned char *>(base + 0x100);
  unsigned char mac[6];
  unsigned int generation;

  do {
    generation = regs[0xfc / 4];

    for (int i = 0; i < 6; ++i) {
      mac[i] = config[i];
    }
  } while (generation != regs[0xfc / 4]);

  const char *digits = "0123456789abcdef";

  uart_puts("MAC = ");
  for (int i = 0; i < 6; ++i) {
    if (i != 0) {
      uart_putc(':');
    }

    uart_putc(digits[mac[i] >> 4]);
    uart_putc(digits[mac[i] & 0xf]);
  }

  uart_puts("\r\n");

  // Select the receive queue.
  regs[0x30 / 4] = 0;
  unsigned int rx_max = regs[0x34 / 4];

  // Select the transmit queue.
  regs[0x30 / 4] = 1;
  unsigned int tx_max = regs[0x34 / 4];

  uart_puts("RX queue max = ");
  uart_put_hex(rx_max);
  uart_puts("\r\n");

  uart_puts("TX queue max = ");
  uart_put_hex(tx_max);
  uart_puts("\r\n");

  if (!setup_virtqueue(regs, 0, rx_desc, &rx_avail, &rx_used) ||
      !setup_virtqueue(regs, 1, tx_desc, &tx_avail, &tx_used)) {
    uart_puts("Queue setup failed\r\n");
    regs[0x70 / 4] = regs[0x70 / 4] | 128;
    return;
  }
  uart_puts("Queue registered\r\n");

  // Set DRIVER_OK
  asm volatile("fence iorw, iorw" ::: "memory");
  regs[0x70 / 4] = regs[0x70 / 4] | 4;

  uart_puts("virtio final status = ");
  uart_put_hex(regs[0x70 / 4]);
  uart_puts("\r\n");
}

extern "C" void kernel_main() {
  uart_puts("Hello!\r\n");
  unsigned long net_base = find_virtio_net();

  if (net_base == 0) {
    uart_puts("virtio-net not found\r\n");
  } else {
    uart_puts("virtio-net at ");
    uart_put_hex(net_base);
    uart_puts("\r\n");
    virtio_net_begin(net_base);
  }
  for (;;) {
  }
}
