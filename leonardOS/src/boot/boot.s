# LeonardOS - Boot Stage (Multiboot2)
# GRUB carrega aqui em 32-bit

.section .multiboot_header, "a"
.align 8

multiboot_header:
    .long 0xe85250d6            /* magic */
    .long 0                     /* architecture: i386 */
    .long (header_end - multiboot_header)  /* header length */
    .long -(0xe85250d6 + 0 + (header_end - multiboot_header))  /* checksum */

    /* End tag (type=0, flags=0, size=8) */
    .align 8
    .short 0    /* type */
    .short 0    /* flags */
    .long  8    /* size */
header_end:

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
