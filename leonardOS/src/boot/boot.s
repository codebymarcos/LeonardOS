# LeonardOS - Boot Stage (Multiboot2)
# GRUB carrega aqui em 32-bit

.section .multiboot_header
.align 4

multiboot_header:
    .long 0xe85250d6
    .long 0
    .long (header_end - multiboot_header)
    .long -(0xe85250d6 + (header_end - multiboot_header))

.align 8
header_end:
    .short 0
    .short 0
    .long 8

.section .bss
.align 4096
stack_bottom:
    .skip 8192
stack_top:

.section .text
.code32

.global _start
_start:
    cli
    mov $stack_top, %esp
    
    push %ebx
    push %eax
    
    call kernel_main_32
    
    cli
    hlt
    jmp .
