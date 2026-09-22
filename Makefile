kernel.elf: boot.o kernel.o linker.ld
	ld.lld -T linker.ld boot.o kernel.o -o kernel.elf

boot.o: boot.S
	clang++ --target=riscv64-unknown-elf -march=rv64imac_zicsr -mabi=lp64 -c boot.S -o boot.o

kernel.o: kernel.cpp
	clang++ --target=riscv64-unknown-elf -march=rv64imac_zicsr -mabi=lp64 -ffreestanding -fno-exceptions -fno-rtti -mcmodel=medany -c kernel.cpp -o kernel.o

.PHONY: run
run: kernel.elf
	qemu-system-riscv64 -machine virt -bios none -smp 1 -m 128M -nographic -kernel kernel.elf \
	-global virtio-mmio.force-legacy=false \
	-netdev user,id=net0 \
	-device virtio-net-device,netdev=net0
