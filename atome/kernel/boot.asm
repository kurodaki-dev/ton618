; Atome kernel — entry point. GRUB (or QEMU's -kernel) loads this ELF through the Multiboot 1 protocol:
; it leaves the CPU in 32-bit protected mode with a flat memory model and hands us
; eax = 0x2BADB002 and ebx = the address of the multiboot info structure.
bits 32

MB_MAGIC   equ 0x1BADB002
MB_FLAGS   equ 0x00000003          ; bit 0: align modules on pages, bit 1: give us the memory map
MB_CHECK   equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECK

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

section .text
global _start
extern kmain
_start:
    cli
    mov esp, stack_top
    xor ebp, ebp
    push ebx                       ; multiboot info
    push eax                       ; magic
    call kmain
.hang:
    cli
    hlt
    jmp .hang
