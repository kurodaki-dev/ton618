# The Atome kernel: 32-bit x86, freestanding (no libc, no libgcc), loaded through Multiboot 1.
CC      ?= gcc
CFLAGS  = -m32 -std=gnu11 -O2 -Wall -Wextra -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
          -fno-builtin -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -nostdlib -static
OBJS    = boot.o kmain.o libk.o console.o input.o time.o tonvm.o

all: atome.elf

atome.elf: $(OBJS) linker.ld
	ld -m elf_i386 -T linker.ld -nostdlib -z noexecstack -o $@ $(OBJS)

boot.o: boot.asm
	nasm -f elf32 $< -o $@

%.o: %.c kernel.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o atome.elf

.PHONY: all clean
