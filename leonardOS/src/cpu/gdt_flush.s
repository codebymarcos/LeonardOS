# LeonardOS - GDT Flush (Assembly)
# Carrega a GDT e recarrega os segment registers

.section .text
.code32

.global gdt_flush

# void gdt_flush(struct gdt_ptr *ptr)
# Argumento na stack: [esp+4] = ponteiro para gdt_ptr
gdt_flush:
    mov 4(%esp), %eax       # Carrega ponteiro para gdt_ptr
    lgdt (%eax)             # Carrega a GDT na CPU

    # Recarrega CS com far jump para o segmento de código (0x08)
    ljmp $0x08, $.reload_cs

.reload_cs:
    # Recarrega todos os data segment registers (0x10 = kernel data)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    ret
