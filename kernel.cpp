// Use C Linkage so the assembly code can call kernel_main without C++ name
// mangling.
extern "C" void kernel_main() {
  for (;;) {
  }
}
