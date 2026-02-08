# LeonardOS - ISR/IRQ Assembly Stubs + IDT Flush
# Stubs que salvam o contexto da CPU e chamam o dispatcher C

.section .text
.code32

# ============================================================
# IDT Flush - Carrega a IDT na CPU
# ============================================================
.global idt_flush
idt_flush:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

# ============================================================
# Dispatcher C (definido em isr.c)
# ============================================================
.extern isr_dispatcher

# ============================================================
# Stub comum - salva contexto, chama dispatcher, restaura
# ============================================================
isr_common_stub:
    pusha                   # Salva EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    # Salva registradores de segmento
    push %ds
    push %es
    push %fs
    push %gs

    # Carrega segmento de dados do kernel
    mov $0x10, %ax          # GDT_KERNEL_DATA_SEG
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    push %esp               # Passa ponteiro para isr_frame como argumento
    call isr_dispatcher
    add $4, %esp            # Limpa argumento

    # Restaura registradores de segmento
    pop %gs
    pop %fs
    pop %es
    pop %ds

    popa                    # Restaura registradores
    add $8, %esp            # Remove int_no e err_code da stack
    iret                    # Retorna da interrupção

# ============================================================
# Exceções da CPU (0-31)
# Algumas pusham error code automaticamente, outras não
# ============================================================

# Macro: exceção SEM error code (pushamos 0 como dummy)
.macro ISR_NOERR num
.global isr_stub_\num
isr_stub_\num:
    push $0                 # Dummy error code
    push $\num              # Número da interrupção
    jmp isr_common_stub
.endm

# Macro: exceção COM error code (CPU já pushou)
.macro ISR_ERR num
.global isr_stub_\num
isr_stub_\num:
    push $\num              # Número da interrupção
    jmp isr_common_stub
.endm

# ISR 0-31: Exceções da CPU
ISR_NOERR 0    # Division By Zero
ISR_NOERR 1    # Debug
ISR_NOERR 2    # NMI
ISR_NOERR 3    # Breakpoint
ISR_NOERR 4    # Overflow
ISR_NOERR 5    # Bound Range
ISR_NOERR 6    # Invalid Opcode
ISR_NOERR 7    # Device Not Available
ISR_ERR   8    # Double Fault
ISR_NOERR 9    # Coprocessor Segment Overrun
ISR_ERR   10   # Invalid TSS
ISR_ERR   11   # Segment Not Present
ISR_ERR   12   # Stack-Segment Fault
ISR_ERR   13   # General Protection Fault
ISR_ERR   14   # Page Fault
ISR_NOERR 15   # Reserved
ISR_NOERR 16   # x87 Floating Point
ISR_ERR   17   # Alignment Check
ISR_NOERR 18   # Machine Check
ISR_NOERR 19   # SIMD Floating Point
ISR_NOERR 20   # Virtualization
ISR_ERR   21   # Control Protection
ISR_NOERR 22   # Reserved
ISR_NOERR 23   # Reserved
ISR_NOERR 24   # Reserved
ISR_NOERR 25   # Reserved
ISR_NOERR 26   # Reserved
ISR_NOERR 27   # Reserved
ISR_NOERR 28   # Hypervisor Injection
ISR_NOERR 29   # VMM Communication
ISR_ERR   30   # Security Exception
ISR_NOERR 31   # Reserved

# ============================================================
# IRQs do PIC (0-15 -> INT 32-47)
# Nenhuma IRQ envia error code
# ============================================================

.macro IRQ_STUB irq_num, int_num
.global irq_stub_\irq_num
irq_stub_\irq_num:
    push $0                 # Dummy error code
    push $\int_num          # Número da interrupção (32+irq)
    jmp isr_common_stub
.endm

IRQ_STUB 0,  32    # Timer (PIT)
IRQ_STUB 1,  33    # Teclado
IRQ_STUB 2,  34    # Cascade (PIC slave)
IRQ_STUB 3,  35    # COM2
IRQ_STUB 4,  36    # COM1
IRQ_STUB 5,  37    # LPT2
IRQ_STUB 6,  38    # Floppy
IRQ_STUB 7,  39    # LPT1 / Spurious
IRQ_STUB 8,  40    # CMOS RTC
IRQ_STUB 9,  41    # Free
IRQ_STUB 10, 42    # Free
IRQ_STUB 11, 43    # Free
IRQ_STUB 12, 44    # PS/2 Mouse
IRQ_STUB 13, 45    # FPU
IRQ_STUB 14, 46    # Primary ATA
IRQ_STUB 15, 47    # Secondary ATA
