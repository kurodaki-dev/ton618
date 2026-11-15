; Atome kernel — entry point. GRUB (or QEMU's -kernel) loads this ELF through the Multiboot 1 protocol:
; it leaves the CPU in 32-bit protected mode with a flat memory model and hands us
; eax = 0x2BADB002 and ebx = the address of the multiboot info structure.
bits 32

MB_MAGIC   equ 0x1BADB002
%ifdef GFX
MB_FLAGS   equ 0x00000007          ; bit 0: align modules, bit 1: memory map, bit 2: ask the loader for a graphical screen
%else
MB_FLAGS   equ 0x00000003          ; bit 0: align modules on pages, bit 1: give us the memory map
%endif
MB_CHECK   equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECK
%ifdef GFX
    dd 0, 0, 0, 0, 0               ; address fields (unused for an ELF kernel)
    dd 0                           ; linear graphics mode
    dd 1024, 768, 32               ; width, height, bits per pixel
%endif

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
