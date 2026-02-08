# LeonardOS - Kernel x86_64 Minimalista

Um kernel x86_64 extremamente simples, estável e pequeno em C e Assembly.

## Filosofia

- **Minimalismo radical**: Menos código = menos erros
- **Assembly apenas onde obrigatório**: Apenas boot, stack inicial e salto para C
- **C simples e previsível**: Sem libc, sem heap, buffers fixos
- **Falhar visivelmente**: Erros são mostrados na tela
- **ASCII puro**: Única interface é VGA text mode 80x25

## Arquitetura

- Kernel monolítico
- x86_64 (64 bits)
- Tudo em kernel space
- Sem multitarefa, sem user space, sem rede, sem filesystem

## Estrutura

```
leonardOS/
├── src/
│   ├── boot.s        # Multiboot2 + inicialização (assembly)
│   ├── kernel.c      # Kernel main (C)
│   ├── vga.c         # Driver VGA text mode (C)
│   ├── keyboard.c    # Driver teclado PS/2 (C)
│   └── shell.c       # Shell ASCII simples (C)
├── leonardos.ld      # Linker script
├── Makefile          # Build automation
└── grub.cfg          # Configuração GRUB2
```

## Componentes

### boot.s (Assembly)
- Cabeçalho Multiboot2 para GRUB
- Inicializa stack (4KB)
- Limpa registradores
- Salta para `kernel_main`

### kernel.c (C)
- Inicializa VGA
- Verifica magic number Multiboot
- Inicia shell

### vga.c (C)
- Text mode 80x25
- Putchar, puts, putint, puthex
- Scroll automático
- Simples e sem estado complexo

### keyboard.c (C)
- PS/2 com polling
- Buffer circular simples
- Scancode -> ASCII
- Leitura linha por linha

### shell.c (C)
- Loop simples de comandos
- Comandos: `help`, `clear`, `halt`
- Expansível para novos comandos

## Como Compilar

```bash
cd leonardOS
make              # Compila kernel.elf
make clean        # Remove arquivos de build
```

Requisitos:
- `gcc` (x86_64)
- `binutils` (ld, as, objcopy)
- `grub-tools` (opcional, para ISO)

## Como Testar

### Com QEMU (recomendado)

```bash
make qemu
```

Requer QEMU instalado. O kernel iniciará e exibirá um shell ASCII.

### Com GRUB/ISO

```bash
make iso
grub-mkrescue -o leonardos.iso build/isodir
```

Depois carregue `leonardos.iso` em QEMU ou outro emulador.

## Interação

Ao iniciar, o kernel mostrará:
```
=== LeonardOS ===
Kernel iniciado com sucesso.
Bootloader: GRUB (Multiboot2)
Arquitetura: x86_64
Iniciando shell...

LeonardOS v0.1 - Digite 'help' para ajuda
>
```

### Comandos Disponíveis

- `help` - Exibe ajuda
- `clear` - Limpa tela
- `halt` - Desliga kernel

## Tamanho

O kernel.elf é **extremamente pequeno** (< 10KB) pois:
- Sem libc
- Sem multitarefa
- Sem filesystem
- Sem drivers complexos
- Código mínimo e direto

## Limitações Intencionais

- ❌ Sem multitarefa
- ❌ Sem user mode
- ❌ Sem proteção de memória
- ❌ Sem interrupts (apenas polling PS/2)
- ❌ Sem rede
- ❌ Sem filesystem
- ❌ Sem IPC
- ✅ Só ASCII em VGA text mode

## Próximos Passos (Opcionais)

Se quiser expandir mantendo minimalismo:

1. **Interrupts**: Implementar IDT/GDT para tratamento de exceções
2. **Novos comandos**: Adicionar comandos na shell (ex: `uptime`, `info`)
3. **Memória**: Detector de RAM via Multiboot
4. **Timer**: PIT para medir tempo decorrido

## Referências

- [OSDev - Multiboot2](https://wiki.osdev.org/Multiboot2)
- [OSDev - VGA Text Mode](https://wiki.osdev.org/Text_mode)
- [OSDev - PS/2 Keyboard](https://wiki.osdev.org/PS/2_Keyboard)
- [x86-64 AMD64 ABI](https://github.com/hjl-tools/x86-psABI)

## Licença

Domínio público. Use como base para seus próprios kernels educacionais.

---

**LeonardOS**: Minimalismo em x86_64. Menos é mais.
