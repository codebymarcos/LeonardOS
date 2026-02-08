// LeonardOS - Driver VGA Text Mode
// Suporte completo UTF-8 -> CP437 + cores + scrollback

#include "vga.h"
#include "../../common/io.h"

// Endereço da memória VGA (80x25 texto)
#define VGA_MEMORY ((unsigned char *)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// ============================================================
// Scrollback buffer
// Armazena todo o histórico de saída (caractere + atributo)
// ============================================================
#define SCROLLBACK_LINES 200

// Buffer: cada célula tem 2 bytes (char + attr), 80 colunas
static unsigned char scrollback[SCROLLBACK_LINES * VGA_WIDTH * 2];

// Linha lógica atual de escrita no scrollback (posição absoluta, cresce sem limite)
static int sb_write_line = 0;

// Total de linhas válidas no scrollback
static int sb_total_lines = 0;

// Offset de visualização: 0 = bottom (acompanhando output), >0 = rolado para cima
static int sb_view_offset = 0;

static int cursor_row = 0;
static int cursor_col = 0;

// Cor global atual (padrão: tema default do colors.h)
static unsigned char current_attr = THEME_DEFAULT;

// Define a cor global
void vga_set_color(unsigned char attr) {
    current_attr = attr;
}

// Retorna a cor global atual
unsigned char vga_get_color(void) {
    return current_attr;
}

// ============================================================
// Tabela de mapeamento Unicode -> CP437
// Cobre acentos latinos, símbolos e box-drawing
// ============================================================

typedef struct {
    unsigned int codepoint;
    unsigned char cp437;
} unicode_map_t;

static const unicode_map_t utf8_to_cp437[] = {
    // Letras acentuadas minúsculas
    { 0x00E0, 0x85 }, // à
    { 0x00E1, 0xA0 }, // á
    { 0x00E2, 0x83 }, // â
    { 0x00E3, 0x84 }, // ã  (usando ä como fallback)
    { 0x00E4, 0x84 }, // ä
    { 0x00E5, 0x86 }, // å
    { 0x00E6, 0x91 }, // æ
    { 0x00E7, 0x87 }, // ç
    { 0x00E8, 0x8A }, // è
    { 0x00E9, 0x82 }, // é
    { 0x00EA, 0x88 }, // ê
    { 0x00EB, 0x89 }, // ë
    { 0x00EC, 0x8D }, // ì
    { 0x00ED, 0xA1 }, // í
    { 0x00EE, 0x8C }, // î
    { 0x00EF, 0x8B }, // ï
    { 0x00F1, 0xA4 }, // ñ
    { 0x00F2, 0x95 }, // ò
    { 0x00F3, 0xA2 }, // ó
    { 0x00F4, 0x93 }, // ô
    { 0x00F5, 0x94 }, // õ  (usando ö como fallback)
    { 0x00F6, 0x94 }, // ö
    { 0x00F9, 0x97 }, // ù
    { 0x00FA, 0xA3 }, // ú
    { 0x00FB, 0x96 }, // û
    { 0x00FC, 0x81 }, // ü

    // Letras acentuadas maiúsculas
    { 0x00C0, 0x41 }, // À -> A
    { 0x00C1, 0x41 }, // Á -> A
    { 0x00C2, 0x41 }, // Â -> A
    { 0x00C3, 0x41 }, // Ã -> A
    { 0x00C4, 0x8E }, // Ä
    { 0x00C5, 0x8F }, // Å
    { 0x00C6, 0x92 }, // Æ
    { 0x00C7, 0x80 }, // Ç
    { 0x00C8, 0x45 }, // È -> E
    { 0x00C9, 0x90 }, // É
    { 0x00CA, 0x45 }, // Ê -> E
    { 0x00CB, 0x45 }, // Ë -> E
    { 0x00CC, 0x49 }, // Ì -> I
    { 0x00CD, 0x49 }, // Í -> I
    { 0x00CE, 0x49 }, // Î -> I
    { 0x00CF, 0x49 }, // Ï -> I
    { 0x00D1, 0xA5 }, // Ñ
    { 0x00D2, 0x4F }, // Ò -> O
    { 0x00D3, 0x4F }, // Ó -> O
    { 0x00D4, 0x4F }, // Ô -> O
    { 0x00D5, 0x4F }, // Õ -> O
    { 0x00D6, 0x99 }, // Ö
    { 0x00D9, 0x55 }, // Ù -> U
    { 0x00DA, 0x55 }, // Ú -> U
    { 0x00DB, 0x55 }, // Û -> U
    { 0x00DC, 0x9A }, // Ü

    // Símbolos comuns
    { 0x00A1, 0xAD }, // ¡
    { 0x00A2, 0x9B }, // ¢
    { 0x00A3, 0x9C }, // £
    { 0x00A5, 0x9D }, // ¥
    { 0x00AA, 0xA6 }, // ª
    { 0x00AB, 0xAE }, // «
    { 0x00AC, 0xAA }, // ¬
    { 0x00B0, 0xF8 }, // °
    { 0x00B1, 0xF1 }, // ±
    { 0x00B2, 0xFD }, // ²
    { 0x00B5, 0xE6 }, // µ
    { 0x00B7, 0xFA }, // ·
    { 0x00BA, 0xA7 }, // º
    { 0x00BB, 0xAF }, // »
    { 0x00BC, 0xAC }, // ¼
    { 0x00BD, 0xAB }, // ½
    { 0x00BF, 0xA8 }, // ¿
    { 0x00D7, 0x78 }, // × -> x
    { 0x00DF, 0xE1 }, // ß
    { 0x00F7, 0xF6 }, // ÷

    // Box-drawing (linhas simples)
    { 0x2500, 0xC4 }, // ─
    { 0x2502, 0xB3 }, // │
    { 0x250C, 0xDA }, // ┌
    { 0x2510, 0xBF }, // ┐
    { 0x2514, 0xC0 }, // └
    { 0x2518, 0xD9 }, // ┘
    { 0x251C, 0xC3 }, // ├
    { 0x2524, 0xB4 }, // ┤
    { 0x252C, 0xC2 }, // ┬
    { 0x2534, 0xC1 }, // ┴
    { 0x253C, 0xC5 }, // ┼

    // Box-drawing (linhas duplas)
    { 0x2550, 0xCD }, // ═
    { 0x2551, 0xBA }, // ║
    { 0x2554, 0xC9 }, // ╔
    { 0x2557, 0xBB }, // ╗
    { 0x255A, 0xC8 }, // ╚
    { 0x255D, 0xBC }, // ╝
    { 0x2560, 0xCC }, // ╠
    { 0x2563, 0xB9 }, // ╣
    { 0x2566, 0xCB }, // ╦
    { 0x2569, 0xCA }, // ╩
    { 0x256C, 0xCE }, // ╬

    // Box-drawing (mistas simples/duplas)
    { 0x2552, 0xD5 }, // ╒
    { 0x2553, 0xD6 }, // ╓
    { 0x2555, 0xB8 }, // ╕
    { 0x2556, 0xB7 }, // ╖
    { 0x2558, 0xD4 }, // ╘
    { 0x2559, 0xD3 }, // ╙
    { 0x255B, 0xBE }, // ╛
    { 0x255C, 0xBD }, // ╜
    { 0x255E, 0xC6 }, // ╞
    { 0x255F, 0xC7 }, // ╟
    { 0x2561, 0xB5 }, // ╡
    { 0x2562, 0xB6 }, // ╢
    { 0x2564, 0xD1 }, // ╤
    { 0x2565, 0xD2 }, // ╥
    { 0x2567, 0xCF }, // ╧
    { 0x2568, 0xD0 }, // ╨

    // Blocos e sombras
    { 0x2588, 0xDB }, // █
    { 0x2591, 0xB0 }, // ░
    { 0x2592, 0xB1 }, // ▒
    { 0x2593, 0xB2 }, // ▓
    { 0x2580, 0xDF }, // ▀
    { 0x2584, 0xDC }, // ▄
    { 0x258C, 0xDD }, // ▌
    { 0x2590, 0xDE }, // ▐

    // Setas
    { 0x2190, 0x1B }, // ←
    { 0x2191, 0x18 }, // ↑
    { 0x2192, 0x1A }, // →
    { 0x2193, 0x19 }, // ↓
    { 0x2194, 0x1D }, // ↔
    { 0x2195, 0x12 }, // ↕

    // Matemáticos e gregos
    { 0x0393, 0xE2 }, // Γ
    { 0x0398, 0xE9 }, // Θ
    { 0x03A3, 0xE4 }, // Σ
    { 0x03A6, 0xE8 }, // Φ
    { 0x03A9, 0xEA }, // Ω
    { 0x03B1, 0xE0 }, // α
    { 0x03B4, 0xEB }, // δ
    { 0x03B5, 0xEE }, // ε
    { 0x03C0, 0xE3 }, // π
    { 0x03C3, 0xE5 }, // σ
    { 0x03C4, 0xE7 }, // τ
    { 0x03C6, 0xED }, // φ
    { 0x2219, 0xF9 }, // ∙
    { 0x221A, 0xFB }, // √
    { 0x221E, 0xEC }, // ∞
    { 0x2229, 0xEF }, // ∩
    { 0x2248, 0xF7 }, // ≈
    { 0x2260, 0xF0 }, // ≠  (usando ≡ slot)
    { 0x2261, 0xF0 }, // ≡
    { 0x2264, 0xF3 }, // ≤
    { 0x2265, 0xF2 }, // ≥

    // Outros
    { 0x263A, 0x01 }, // ☺
    { 0x263B, 0x02 }, // ☻
    { 0x2665, 0x03 }, // ♥
    { 0x2666, 0x04 }, // ♦
    { 0x2663, 0x05 }, // ♣
    { 0x2660, 0x06 }, // ♠
    { 0x2022, 0x07 }, // •
    { 0x25CB, 0x09 }, // ○
    { 0x266A, 0x0D }, // ♪
    { 0x266B, 0x0E }, // ♫
    { 0x25BA, 0x10 }, // ►
    { 0x25C4, 0x11 }, // ◄
    { 0x25B2, 0x1E }, // ▲
    { 0x25BC, 0x1F }, // ▼

    { 0, 0 } // Terminador
};

// Decodifica um codepoint UTF-8 a partir da string
// Retorna o codepoint e avança o ponteiro
static unsigned int utf8_decode(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    unsigned int cp;
    int extra;

    if (p[0] < 0x80) {
        // ASCII puro (0xxxxxxx)
        *s = (const char *)(p + 1);
        return p[0];
    } else if ((p[0] & 0xE0) == 0xC0) {
        // 2 bytes (110xxxxx 10xxxxxx)
        cp = p[0] & 0x1F;
        extra = 1;
    } else if ((p[0] & 0xF0) == 0xE0) {
        // 3 bytes (1110xxxx 10xxxxxx 10xxxxxx)
        cp = p[0] & 0x0F;
        extra = 2;
    } else if ((p[0] & 0xF8) == 0xF0) {
        // 4 bytes (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
        cp = p[0] & 0x07;
        extra = 3;
    } else {
        // Byte invalido, pula
        *s = (const char *)(p + 1);
        return '?';
    }

    for (int i = 1; i <= extra; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            // Sequencia mal-formada
            *s = (const char *)(p + 1);
            return '?';
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }

    *s = (const char *)(p + 1 + extra);
    return cp;
}

// Converte codepoint Unicode para CP437
static unsigned char unicode_to_cp437(unsigned int cp) {
    // ASCII direto
    if (cp < 0x80) {
        return (unsigned char)cp;
    }

    // Busca na tabela
    for (int i = 0; utf8_to_cp437[i].codepoint != 0; i++) {
        if (utf8_to_cp437[i].codepoint == cp) {
            return utf8_to_cp437[i].cp437;
        }
    }

    // Caractere desconhecido
    return '?';
}

// Calcula posição na memória VGA
static int vga_index(int row, int col) {
    return (row * VGA_WIDTH + col) * 2;
}

// ============================================================
// Scrollback: funções internas
// ============================================================

// Índice no buffer circular do scrollback para uma linha lógica
static int sb_line_index(int logical_line) {
    return ((logical_line % SCROLLBACK_LINES) * VGA_WIDTH * 2);
}

// Escreve uma célula no scrollback na posição (row, col) relativa ao cursor_row lógico
static void sb_write_cell(int row, int col, unsigned char ch, unsigned char attr) {
    int line = sb_write_line - (VGA_HEIGHT - 1) + row;
    if (line < 0) line = 0;
    int idx = sb_line_index(line) + col * 2;
    scrollback[idx] = ch;
    scrollback[idx + 1] = attr;
}

// Limpa uma linha do scrollback
static void sb_clear_line(int logical_line) {
    int idx = sb_line_index(logical_line);
    for (int c = 0; c < VGA_WIDTH; c++) {
        scrollback[idx + c * 2] = ' ';
        scrollback[idx + c * 2 + 1] = current_attr;
    }
}

// Atualiza o cursor de hardware do VGA
static void vga_update_cursor(void) {
    // Se estamos rolados para cima, esconde o cursor
    if (sb_view_offset > 0) {
        // Posiciona cursor fora da tela
        unsigned short pos = VGA_WIDTH * VGA_HEIGHT;
        outb(0x3D4, 14);
        outb(0x3D5, (pos >> 8) & 0xFF);
        outb(0x3D4, 15);
        outb(0x3D5, pos & 0xFF);
    } else {
        unsigned short pos = cursor_row * VGA_WIDTH + cursor_col;
        outb(0x3D4, 14);
        outb(0x3D5, (pos >> 8) & 0xFF);
        outb(0x3D4, 15);
        outb(0x3D5, pos & 0xFF);
    }
}

// Redesenha a tela VGA a partir do scrollback, com o offset de visualização
static void vga_refresh_from_scrollback(void) {
    unsigned char *vga = VGA_MEMORY;

    // A linha do fundo do scrollback é sb_write_line
    // Queremos mostrar as linhas: (sb_write_line - sb_view_offset - VGA_HEIGHT + 1) até (sb_write_line - sb_view_offset)
    int bottom_line = sb_write_line - sb_view_offset;
    int top_line = bottom_line - VGA_HEIGHT + 1;

    for (int screen_row = 0; screen_row < VGA_HEIGHT; screen_row++) {
        int logical_line = top_line + screen_row;
        if (logical_line < 0 || logical_line > sb_write_line) {
            // Linha fora do range: preenche com espaços
            for (int c = 0; c < VGA_WIDTH; c++) {
                vga[(screen_row * VGA_WIDTH + c) * 2] = ' ';
                vga[(screen_row * VGA_WIDTH + c) * 2 + 1] = current_attr;
            }
        } else {
            int idx = sb_line_index(logical_line);
            for (int c = 0; c < VGA_WIDTH; c++) {
                vga[(screen_row * VGA_WIDTH + c) * 2] = scrollback[idx + c * 2];
                vga[(screen_row * VGA_WIDTH + c) * 2 + 1] = scrollback[idx + c * 2 + 1];
            }
        }
    }

    vga_update_cursor();
}

// Limpa a tela e o scrollback (usa cor atual)
void vga_clear(void) {
    unsigned char *vga = VGA_MEMORY;
    
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = current_attr;
    }

    // Limpa scrollback
    for (int i = 0; i < SCROLLBACK_LINES * VGA_WIDTH * 2; i += 2) {
        scrollback[i] = ' ';
        scrollback[i + 1] = current_attr;
    }
    
    sb_write_line = 0;
    sb_total_lines = 0;
    sb_view_offset = 0;
    cursor_row = 0;
    cursor_col = 0;
    vga_update_cursor();
}

// Escreve um byte CP437 com atributo de cor específico (uso interno)
static void vga_putbyte_attr(unsigned char c, unsigned char attr) {
    unsigned char *vga = VGA_MEMORY;

    // Qualquer escrita volta para o fundo do scrollback
    if (sb_view_offset > 0) {
        sb_view_offset = 0;
        vga_refresh_from_scrollback();
    }
    
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            int idx = vga_index(cursor_row, cursor_col);
            vga[idx] = ' ';
            vga[idx + 1] = attr;
            sb_write_cell(cursor_row, cursor_col, ' ', attr);
        }
    } else {
        int idx = vga_index(cursor_row, cursor_col);
        vga[idx] = c;
        vga[idx + 1] = attr;
        sb_write_cell(cursor_row, cursor_col, c, attr);
        cursor_col++;
    }

    // Wrap de coluna
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    
    if (cursor_row >= VGA_HEIGHT) {
        // Avança o scrollback
        sb_write_line++;
        if (sb_total_lines < SCROLLBACK_LINES) {
            sb_total_lines++;
        }

        // Limpa a nova linha no scrollback
        sb_clear_line(sb_write_line);

        // Scroll up na tela VGA
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga[i * 2] = vga[(i + VGA_WIDTH) * 2];
            vga[i * 2 + 1] = vga[(i + VGA_WIDTH) * 2 + 1];
        }
        // Limpa última linha na tela
        for (int i = 0; i < VGA_WIDTH; i++) {
            vga[((VGA_HEIGHT - 1) * VGA_WIDTH + i) * 2] = ' ';
            vga[((VGA_HEIGHT - 1) * VGA_WIDTH + i) * 2 + 1] = current_attr;
        }
        cursor_row = VGA_HEIGHT - 1;
    }

    vga_update_cursor();
}

// Escreve um byte CP437 com a cor global atual (uso interno)
static void vga_putbyte(unsigned char c) {
    vga_putbyte_attr(c, current_attr);
}

// Escreve um caractere (API publica, usa cor global)
void vga_putchar(char c) {
    vga_putbyte((unsigned char)c);
}

// Escreve um caractere com cor específica (não altera cor global)
void vga_putchar_color(char c, unsigned char attr) {
    vga_putbyte_attr((unsigned char)c, attr);
}

// Escreve string com decodificacao UTF-8 -> CP437 (usa cor global)
void vga_puts(const char *s) {
    while (*s) {
        unsigned int cp = utf8_decode(&s);
        unsigned char ch = unicode_to_cp437(cp);
        vga_putbyte(ch);
    }
}

// Escreve string com cor específica (não altera cor global)
void vga_puts_color(const char *s, unsigned char attr) {
    while (*s) {
        unsigned int cp = utf8_decode(&s);
        unsigned char ch = unicode_to_cp437(cp);
        vga_putbyte_attr(ch, attr);
    }
}

// Escreve numero inteiro
void vga_putint(long x) {
    if (x < 0) {
        vga_putbyte('-');
        x = -x;
    }
    
    char buf[20];
    int i = 0;
    
    if (x == 0) {
        vga_putbyte('0');
        return;
    }
    
    while (x > 0) {
        buf[i++] = '0' + (x % 10);
        x /= 10;
    }
    
    while (i > 0) {
        vga_putbyte(buf[--i]);
    }
}

// Escreve hexadecimal
void vga_puthex(unsigned long x) {
    vga_puts("0x");
    char buf[16];
    int i = 0;
    
    if (x == 0) {
        vga_putbyte('0');
        return;
    }
    
    while (x > 0) {
        int digit = x % 16;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        x /= 16;
    }
    
    while (i > 0) {
        vga_putbyte(buf[--i]);
    }
}

// ============================================================
// Scroll público (Page Up / Page Down)
// ============================================================

// Rola N linhas para cima (mostra histórico)
void vga_scroll_up(int lines) {
    // Máximo que podemos rolar = total de linhas salvas - as visíveis
    int max_offset = sb_write_line - VGA_HEIGHT + 1;
    if (max_offset < 0) max_offset = 0;

    sb_view_offset += lines;
    if (sb_view_offset > max_offset) {
        sb_view_offset = max_offset;
    }

    vga_refresh_from_scrollback();
}

// Rola N linhas para baixo (volta ao presente)
void vga_scroll_down(int lines) {
    sb_view_offset -= lines;
    if (sb_view_offset < 0) {
        sb_view_offset = 0;
    }

    vga_refresh_from_scrollback();
}

// Volta imediatamente para o fundo (output atual)
void vga_scroll_to_bottom(void) {
    if (sb_view_offset > 0) {
        sb_view_offset = 0;
        vga_refresh_from_scrollback();
    }
}
