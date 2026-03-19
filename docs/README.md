# LeonardOS

Kernel de sistema operacional bare-metal para x86 32-bit. C freestanding + assembly GNU, Multiboot2, GRUB, QEMU.

## Compilação e Execução

```bash
cd leonardOS
make clean && make   # compila kernel.elf
make run             # compila ISO + executa no QEMU
```

Todos os objetos vão para `build/`. O Makefile tem regras explícitas por arquivo — ao adicionar um novo `.c`/`.s`, você deve adicionar sua variável source, variável object, entrada `OBJ_ALL`, e regra de compilação.

## Arquitetura (ordem de boot)

`boot.s` → `kernel_main_32()`: GDT → PIC (remapeia IRQ 0-15 → INT 32-47) → ISR/IDT (256 entradas) → teclado (IRQ1) → PMM → VMM (mapeamento identidade 16MB) → Heap → VFS + RamFS → IDE/ATA + LeonFS → `sti` → `shell_loop()`

## Convenções Principais

### Ambiente freestanding
- Tipos de `src/common/types.h` (uint8_t..uint64_t, size_t, bool, NULL)
- Portas I/O via `src/common/io.h` (inline `inb`, `outb`, `io_wait`)
- Sem malloc — todos os buffers são estáticos/pilha. Heap implementado mas opcional.
- Flags do compilador: `-ffreestanding -fno-stack-protector -fno-builtin -fno-common -m32`

### Adicionando um comando shell (sistema modular em `src/commands/`)
1. Criar `src/commands/cmd_foo.h` — declarar `void cmd_foo(const char *args);`
2. Criar `src/commands/cmd_foo.c` — implementar, incluir `"commands.h"` se precisar de acesso ao registro
3. Em `src/commands/commands.c` — adicionar `#include "cmd_foo.h"` e entrada na `command_table[]`
4. Em `Makefile` — adicionar variável source, variável object, entrada `OBJ_ALL`, e regra de compilação
5. Padrão: cada handler recebe `const char *args` (texto após o nome do comando). Use `(void)args;` se não usado.

### Saída VGA (src/drivers/vga/)
- Strings UTF-8 no código C são convertidas automaticamente para CP437 no VGA. Use box-drawing Unicode livremente.
- Cores: incluir `common/colors.h`, usar constantes `THEME_*` (THEME_DEFAULT, THEME_ERROR, THEME_BOOT_OK, etc.)
- `vga_puts_color("texto", THEME_X)` para saída colorida sem alterar estado global.
- VGA tem buffer de scrollback de 200 linhas. Page Up/Down tratados transparentemente em `kbd_read_line`.

### Caminhos de include
`-I src` está definido, então de `kernel.c` use `"drivers/vga/vga.h"`. De arquivos mais profundos use relativo: `"../../common/types.h"`.

### Arquivos assembly
- Sintaxe GNU AT&T, flag `--32`
- Stubs assembly (ISR, GDT flush) usam `.global` e são `extern`'d do C
- Stubs ISR empurram código de erro + número int, então pulam para stub comum que salva todos os regs (incluindo ds/es/fs/gs) e chama `isr_dispatcher` em C

### Testes
O comando `test` executa validação automatizada de todos os subsistemas: registradores CPU, entradas GDT, entradas IDT, máscaras PIC, buffer VGA, memória, teclado, portas I/O, e registro de comandos. Sempre execute `test` no QEMU após mudanças.

## Descobertas no x86
- Byte de acesso GDT tem bit Accessed definido pela CPU (0x92→0x93, 0x9A→0x9B) — testes devem aceitar ambos os valores
- `idt_init()` zera a tabela — nunca chame depois de preencher entradas; use `idt_load()` para recarregar sem zerar
- Stub comum ISR deve salvar/restaurar registradores de segmento (ds/es/fs/gs) e carregar segmento de dados do kernel (0x10) antes de chamar código C, ou você terá triple fault
- Pilha é 8KB em BSS (`boot.s`), cresce para baixo — ESP pode ficar abaixo de 0x100000
- Frames do heap do PMM não são contíguos — use `map_page()` para criar range virtual contíguo

## Componentes

### Gerenciamento de Memória
- **PMM**: Alocador bitmap, frames de 4KB, máximo 256MB, mmap Multiboot2
- **VMM**: Mapeamento identidade 0-16MB, handler de page fault (INT 14)
- **Heap**: kmalloc/kfree, lista ligada com first-fit, coalescing, alinhamento de 8 bytes

### Drivers
- **VGA**: Modo texto 80x25, UTF-8→CP437, 16 cores, scrollback de 200 linhas, sistema de captura para pipes
- **Teclado**: PS/2 com scancodes estendidos (Page Up/Down, setas, Home/End), atalhos Ctrl, modo raw para diagnóstico
- **IDE/ATA**: PIO LBA28, disco de 254MB com LeonFS
- **Rede** (planejado): RTL8139 com IRQ + DMA

### Sistema de Arquivos
- **VFS**: Camada virtual abstrata
- **RamFS**: Sistema de arquivos em RAM para root `/`
- **LeonFS**: Sistema de arquivos on-disk com blocos indiretos (~69KB max por arquivo), mount em `/mnt`

### Rede (planejado)
- **Ethernet**: Frames, endereços MAC
- **IP Stack**: ARP, IP, ICMP
- **Transporte**: UDP/TCP básico
- **API**: Sockets
- **Comandos**: ping, ifconfig, netstat

### Scripting
- Engine completa em `src/shell/script.c`: controle de fluxo (if/else, while, for), funções, execução de arquivos `.sh`
- Integração com shell: expansão de variáveis, globs, pipes, ponto-e-vírgula

### CPU
- **GDT**: 3 segmentos planos de 4GB (null, code 0x9A/0xCF, data 0x92/0xCF)
- **IDT**: Tabela de 256 entradas, dispatcher ISR para exceções + IRQs
- **PIC**: 8259 remapeado para INT 32-47

### Shell
- Sistema de comandos modular: help, clear, sysinfo, halt, test, mem, df, ls, cat, echo, pwd, cd, mkdir, touch, rm, cp, reboot, stat, tree, find, grep, env, wc, head, source, keytest
- `mem` mostra estatísticas PMM + Heap com barras visuais
- `test` valida todos os subsistemas (26 comandos registrados)
- Histórico circular de 32 entradas (setas up/down)
- Pipes `|` com captura VGA (até 8 estágios)
- Ponto-e-vírgula `;` para múltiplos comandos
- Variáveis de ambiente `$VAR`, `$?` para código de saída, atribuição `NAME=value`
- Expansão de globs `* ?`
- Engine de scripts: if/elif/else/fi, while/do/done, for/in/do/done, definições de função
- Comando `source` para executar scripts `.sh`
- Atalhos de teclado: Ctrl+Up/Down para scroll, Ctrl+A para `;`
- Echo suporta `>` (sobrescrever) e `>>` (anexar), strings entre aspas, sequências de escape

## Estrutura de Arquivos

```
leonardOS/
├── Makefile
├── README.md
├── build/              # Gerado
├── config/
│   ├── grub.cfg
│   └── leonardos.ld    # Script do linker
├── docs/
│   └── README.md       # Este arquivo
├── src/
│   ├── boot/
│   │   └── boot.s      # Entrada Multiboot2
│   ├── common/
│   │   ├── colors.h    # Temas de cores VGA
│   │   ├── io.h        # I/O de portas
│   │   ├── string.c    # Funções de string
│   │   └── types.h     # Tipos padrão
│   ├── commands/       # Comandos shell (26 comandos)
│   ├── cpu/            # GDT, IDT, ISR
│   ├── drivers/        # VGA, teclado, IDE/ATA
│   ├── fs/             # VFS, RamFS, LeonFS
│   ├── kernel/
│   │   └── kernel.c    # Sequência principal de boot
│   ├── memory/         # PMM, VMM, Heap
│   └── shell/
│       ├── shell.c     # Loop de comandos + features avançadas
│       └── script.c    # Engine de scripts
└── list.txt            # Checklist de features
```

## Princípios de Saúde do codigo

> Se uma decisão não reduz bugs hoje, ela provavelmente não é saudável agora.

> Se você não consegue testar uma camada sozinha, ela não está pronta para a próxima.

> Heap é a última camada "invisível". Depois dele, tudo vira feature visível.
