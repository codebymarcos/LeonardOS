# LeonardOS — Copilot Instructions

## What is this?
Bare-metal x86 **32-bit** OS kernel. Freestanding C + GNU assembly, Multiboot2, GRUB, QEMU. **No libc, no stdlib, no heap (yet).**

## Build & Run
```bash
cd leonardOS
make clean && make   # build kernel.elf
make run             # build ISO + launch QEMU (qemu-system-x86_64 -cdrom)
```
All objects go to `build/`. The Makefile has **explicit per-file rules** — when adding a new `.c`/`.s` file, you must add its source var, object var, `OBJ_ALL` entry, and build rule.

## Architecture (boot order)
`boot.s` → `kernel_main_32()`: GDT → PIC (remap IRQ 0-15 → INT 32-47) → ISR/IDT (256 entries) → keyboard (IRQ1) → `sti` → `shell_loop()`

## Key Conventions

### Freestanding environment
- Types come from `src/common/types.h` (uint8_t..uint64_t, size_t, bool, NULL)
- I/O ports via `src/common/io.h` (inline `inb`, `outb`, `io_wait`)
- **No malloc** — all buffers are static/stack. Heap is planned but not implemented.
- Compiler flags: `-ffreestanding -fno-stack-protector -fno-builtin -fno-common -m32`

### Adding a shell command (modular system in `src/commands/`)
1. Create `src/commands/cmd_foo.h` — declare `void cmd_foo(const char *args);`
2. Create `src/commands/cmd_foo.c` — implement it, include `"commands.h"` if you need registry access
3. In `src/commands/commands.c` — add `#include "cmd_foo.h"` and entry to `command_table[]`
4. In `Makefile` — add source var, object var, `OBJ_ALL` entry, and build rule
5. Pattern: every handler receives `const char *args` (text after command name). Use `(void)args;` if unused.

### VGA output (src/drivers/vga/)
- UTF-8 strings in C source are auto-converted to CP437 for VGA. Use Unicode box-drawing (╔═╗║╚╝) freely.
- Colors: include `common/colors.h`, use `THEME_*` constants (THEME_DEFAULT, THEME_ERROR, THEME_BOOT_OK, etc.)
- `vga_puts_color("text", THEME_X)` for colored output without changing global state.
- VGA has 200-line scrollback buffer. Page Up/Down handled transparently in `kbd_read_line`.

### Include paths
`-I src` is set, so from `kernel.c` use `"drivers/vga/vga.h"`. From deeper files use relative: `"../../common/types.h"`.

### Assembly files
- GNU AT&T syntax, `--32` flag
- Assembly stubs (ISR, GDT flush) use `.global` and are `extern`'d from C
- ISR stubs push error code + int number, then jump to common stub that saves all regs (including ds/es/fs/gs) and calls `isr_dispatcher` in C

### Testing
The `test` command (`src/commands/cmd_test.c`) runs automated validation of all subsystems: CPU registers, GDT entries, IDT entries, PIC masks, VGA buffer, memory, keyboard, I/O ports, and command registry. **Always run `test` in QEMU after changes.**

## x86 gotchas discovered in this project
- GDT access byte gets Accessed bit set by CPU (0x92→0x93, 0x9A→0x9B) — tests must accept both values
- `idt_init()` zeros the table — never call it after filling entries; use `idt_load()` to reload without zeroing
- ISR common stub must save/restore segment registers (ds/es/fs/gs) and load kernel data segment (0x10) before calling C code, or you get a triple fault
- Stack is 8KB in BSS (`boot.s`), grows downward — ESP can be below 0x100000
### 🧩 Regra de ouro

> Se uma decisão **não reduz bugs hoje**, ela provavelmente **não é saudável agora**.

## 🧩 Regra de ouro (continua valendo)

> Se você não consegue testar uma camada sozinha, ela não está pronta para a próxima.

🧩 Regra de ouro (continua valendo)

Heap é a última camada “invisível”.
Depois dele, tudo vira feature visível.