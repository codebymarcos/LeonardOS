// LeonardOS - Driver Teclado PS/2
// Baseado em IRQ1 (interrupção de hardware)

#include "keyboard.h"
#include "../vga/vga.h"
#include "../pic/pic.h"
#include "../../cpu/isr.h"
#include "../../common/io.h"

// Portas I/O PS/2
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

// Buffer circular de teclado
#define KBD_BUFFER_SIZE 256
static volatile unsigned char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

// Flags de estado de teclas modificadoras
static volatile int kbd_extended = 0;   // Prefixo 0xE0 (teclas estendidas)
static volatile int kbd_shift = 0;      // Shift pressionado
static volatile int kbd_ctrl = 0;       // Ctrl pressionado
static volatile int kbd_caps = 0;       // Caps Lock ativo

// Scancodes dos modificadores
#define SC_LSHIFT_PRESS   0x2A
#define SC_RSHIFT_PRESS   0x36
#define SC_LSHIFT_RELEASE 0xAA
#define SC_RSHIFT_RELEASE 0xB6
#define SC_CTRL_PRESS     0x1D
#define SC_CTRL_RELEASE   0x9D
#define SC_CAPSLOCK       0x3A

// Mapa de scancodes para ASCII (US layout — sem Shift)
static const char scancode_map[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b',  'n', 'm', ',', '.', '/',  0,  '*', 0,  ' ', 0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2',  '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Mapa de scancodes para ASCII (US layout — com Shift)
static const char scancode_shift_map[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D',  'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '"', 0,  '|', 'Z', 'X', 'C', 'V',
    'B',  'N', 'M', '<', '>', '?',  0,  '*', 0,  ' ', 0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2',  '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Adiciona caractere ao buffer
static void kbd_enqueue(unsigned char c) {
    if (c == 0) return;
    
    int next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next_head != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next_head;
    }
}

// ============================================================
// Modo diagnóstico (raw scancode)
// ============================================================
static int raw_mode = 0;
static volatile unsigned char raw_scancode = 0;

void kbd_set_raw_mode(int enabled) {
    raw_mode = enabled;
    raw_scancode = 0;
}

unsigned char kbd_get_raw_scancode(void) {
    unsigned char sc = raw_scancode;
    raw_scancode = 0;
    return sc;
}

// Handler de interrupção IRQ1 - chamado automaticamente pelo PIC
static void kbd_irq_handler(struct isr_frame *frame) {
    (void)frame;

    unsigned char scancode = inb(KBD_DATA_PORT);

    // Modo diagnóstico: captura scancode bruto e encaminha para o buffer
    if (raw_mode) {
        raw_scancode = scancode;
        // Ainda precisa ler o scancode para limpar o buffer do teclado
        // Mas não processa nada — só armazena
        // Enqueue ESC para acordar kbd_getchar() no cmd_keytest
        if (!(scancode & 0x80) && scancode != 0xE0) {
            kbd_enqueue(0x01);  // sinal dummy para acordar o loop
        }
        return;
    }

    // Prefixo de tecla estendida
    if (scancode == 0xE0) {
        kbd_extended = 1;
        return;
    }

    // --- Teclas modificadoras (press e release) ---
    // Shift press
    if (scancode == SC_LSHIFT_PRESS || scancode == SC_RSHIFT_PRESS) {
        kbd_shift = 1;
        return;
    }
    // Shift release
    if (scancode == SC_LSHIFT_RELEASE || scancode == SC_RSHIFT_RELEASE) {
        kbd_shift = 0;
        return;
    }
    // Ctrl press
    if (scancode == SC_CTRL_PRESS) {
        kbd_ctrl = 1;
        return;
    }
    // Ctrl release (0x1D + 0x80 = 0x9D)
    if (scancode == SC_CTRL_RELEASE) {
        kbd_ctrl = 0;
        return;
    }
    // Caps Lock (toggle no press)
    if (scancode == SC_CAPSLOCK) {
        kbd_caps = !kbd_caps;
        return;
    }

    // Ignora key release (bit 7 = 1) para teclas normais
    if (scancode & 0x80) {
        kbd_extended = 0;
        return;
    }

    if (kbd_extended) {
        kbd_extended = 0;
        // Scancodes estendidos (após 0xE0)
        switch (scancode) {
            case 0x49: kbd_enqueue(KEY_PAGE_UP);   return;
            case 0x51: kbd_enqueue(KEY_PAGE_DOWN); return;
            case 0x48:
                if (kbd_ctrl) kbd_enqueue(KEY_CTRL_UP);
                else kbd_enqueue(KEY_ARROW_UP);
                return;
            case 0x50:
                if (kbd_ctrl) kbd_enqueue(KEY_CTRL_DOWN);
                else kbd_enqueue(KEY_ARROW_DOWN);
                return;
            case 0x47: kbd_enqueue(KEY_HOME);      return;
            case 0x4F: kbd_enqueue(KEY_END);       return;
            default: return;
        }
    }

    if (scancode < 128) {
        // Atalhos Ctrl+Shift+tecla
        if (kbd_ctrl && kbd_shift) {
            switch (scancode) {
                case 0x1E: kbd_enqueue(':'); return;  // Ctrl+Shift+A = :
            }
        }

        // Atalhos Ctrl+tecla
        if (kbd_ctrl) {
            switch (scancode) {
                case 0x1E: kbd_enqueue(';'); return;  // Ctrl+A = ;
            }
        }

        // Decide qual mapa usar
        int use_shift = kbd_shift;

        // Caps Lock afeta apenas letras (a-z)
        char base = scancode_map[scancode];
        if (kbd_caps && base >= 'a' && base <= 'z') {
            use_shift = !use_shift;  // Caps inverte o shift para letras
        }

        char c = use_shift ? scancode_shift_map[scancode] : scancode_map[scancode];
        kbd_enqueue((unsigned char)c);
    }
}

// Inicializa o driver de teclado (registra IRQ1)
void kbd_init(void) {
    // Registra handler para IRQ1 (INT 33)
    isr_register_handler(IRQ_TO_INT(IRQ_KEYBOARD), kbd_irq_handler);

    // Habilita IRQ1 no PIC
    pic_unmask_irq(IRQ_KEYBOARD);
}

// Lê um caractere do buffer (bloqueia até haver dados)
char kbd_getchar(void) {
    while (kbd_head == kbd_tail) {
        // Espera interrupção (CPU descansa até a próxima IRQ)
        asm volatile("hlt");
    }
    
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

// Verifica se há caractere pronto
int kbd_has_char(void) {
    return kbd_head != kbd_tail;
}

// ============================================================
// Histórico de comandos
// ============================================================
#define HISTORY_SIZE 32
#define HISTORY_LINE_MAX 256

static char history[HISTORY_SIZE][HISTORY_LINE_MAX];
static int history_count = 0;     // Quantas entradas já existem
static int history_write = 0;     // Próxima posição de escrita (circular)

// Adiciona uma linha ao histórico
static void history_add(const char *line) {
    if (!line || line[0] == '\0') return;

    // Não duplica se igual ao último
    if (history_count > 0) {
        int last = (history_write - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        // Compara manualmente
        const char *a = history[last];
        const char *b = line;
        int same = 1;
        while (*a && *b) {
            if (*a != *b) { same = 0; break; }
            a++; b++;
        }
        if (*a != *b) same = 0;
        if (same) return;
    }

    // Copia para o buffer
    int i = 0;
    while (line[i] && i < HISTORY_LINE_MAX - 1) {
        history[history_write][i] = line[i];
        i++;
    }
    history[history_write][i] = '\0';

    history_write = (history_write + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) history_count++;
}

// Lê uma linha (até newline) com echo, scroll e histórico Up/Down
void kbd_read_line(char *buf, int maxlen) {
    int i = 0;
    int hist_pos = -1;  // -1 = digitando novo, 0..count-1 = navegando

    // Salva o que o usuário estava digitando antes de navegar
    char saved_input[HISTORY_LINE_MAX];
    saved_input[0] = '\0';

    while (i < maxlen - 1) {
        char c = kbd_getchar();

        // Teclas especiais de scroll (tratadas transparentemente)
        if ((unsigned char)c == KEY_PAGE_UP) {
            vga_scroll_up(5);
            continue;
        }
        if ((unsigned char)c == KEY_PAGE_DOWN) {
            vga_scroll_down(5);
            continue;
        }
        if ((unsigned char)c == KEY_CTRL_UP) {
            vga_scroll_up(1);
            continue;
        }
        if ((unsigned char)c == KEY_CTRL_DOWN) {
            vga_scroll_down(1);
            continue;
        }
        if ((unsigned char)c == KEY_HOME) {
            vga_scroll_up(200);
            continue;
        }
        if ((unsigned char)c == KEY_END) {
            vga_scroll_to_bottom();
            continue;
        }

        // Arrow Up — histórico anterior
        if ((unsigned char)c == KEY_ARROW_UP) {
            vga_scroll_to_bottom();
            if (history_count == 0) continue;

            // Salva input atual se acabamos de começar a navegar
            if (hist_pos == -1) {
                int si = 0;
                while (si < i && si < HISTORY_LINE_MAX - 1) {
                    saved_input[si] = buf[si];
                    si++;
                }
                saved_input[si] = '\0';
            }

            if (hist_pos == -1) {
                hist_pos = 0;
            } else if (hist_pos < history_count - 1) {
                hist_pos++;
            } else {
                continue; // Já no mais antigo
            }

            // Apaga a linha atual na tela
            while (i > 0) {
                vga_putchar('\b');
                i--;
            }

            // Copia do histórico (mais recente = hist_pos 0)
            int idx = (history_write - 1 - hist_pos + HISTORY_SIZE) % HISTORY_SIZE;
            i = 0;
            while (history[idx][i] && i < maxlen - 1) {
                buf[i] = history[idx][i];
                vga_putchar(buf[i]);
                i++;
            }
            buf[i] = '\0';
            continue;
        }

        // Arrow Down — histórico mais recente
        if ((unsigned char)c == KEY_ARROW_DOWN) {
            vga_scroll_to_bottom();
            if (hist_pos <= -1) continue;

            // Apaga a linha atual na tela
            while (i > 0) {
                vga_putchar('\b');
                i--;
            }

            hist_pos--;
            if (hist_pos < 0) {
                // Volta ao input original
                hist_pos = -1;
                i = 0;
                while (saved_input[i] && i < maxlen - 1) {
                    buf[i] = saved_input[i];
                    vga_putchar(buf[i]);
                    i++;
                }
                buf[i] = '\0';
            } else {
                int idx = (history_write - 1 - hist_pos + HISTORY_SIZE) % HISTORY_SIZE;
                i = 0;
                while (history[idx][i] && i < maxlen - 1) {
                    buf[i] = history[idx][i];
                    vga_putchar(buf[i]);
                    i++;
                }
                buf[i] = '\0';
            }
            continue;
        }

        // Qualquer tecla de texto volta ao fundo
        vga_scroll_to_bottom();

        if (c == '\n' || c == '\r') {
            break;
        }
        if (c == '\b') {
            if (i > 0) {
                i--;
                vga_putchar('\b');
            }
        } else if (c >= 32 && c < 127) {
            buf[i++] = c;
            vga_putchar(c);
            hist_pos = -1;  // Reset histórico ao digitar
        }
    }
    buf[i] = '\0';

    // Salva no histórico
    history_add(buf);
}
