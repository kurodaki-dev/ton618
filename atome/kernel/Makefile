# The Atome kernel: 32-bit x86, freestanding (no libc, no libgcc), loaded through Multiboot 1.
CC      ?= gcc
CFLAGS  = -m32 -std=gnu11 -O2 -Wall -Wextra -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
          -fno-builtin -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -nostdlib -static
OBJS    = boot.o kmain.o libk.o console.o input.o time.o disk.o gfx.o tonvm.o
COMMON  = kmain.o libk.o console.o input.o time.o disk.o gfx.o tonvm.o

all: atome.elf atome-gfx.elf

atome.elf: $(OBJS) linker.ld
	ld -m elf_i386 -T linker.ld -nostdlib -z noexecstack -o $@ $(OBJS)

boot.o: boot.asm
	nasm -f elf32 $< -o $@

# the same kernel, asking the loader (GRUB) for a 1024x768 graphical screen
boot-gfx.o: boot.asm
	nasm -f elf32 -DGFX $< -o $@

atome-gfx.elf: boot-gfx.o $(COMMON) linker.ld
	ld -m elf_i386 -T linker.ld -nostdlib -z noexecstack -o $@ boot-gfx.o $(COMMON)

%.o: %.c kernel.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o atome.elf atome-gfx.elf

.PHONY: all clean
