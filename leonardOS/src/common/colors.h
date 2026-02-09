// LeonardOS - Definições de cores VGA
// Arquivo central de cores - importe de qualquer lugar do projeto
//
// Uso:
//   #include "common/colors.h"   (do kernel)
//   #include "../common/colors.h" (de drivers/shell)
//
// As 16 cores do VGA Text Mode (4 bits cada):
//   Foreground (texto): 0-15 (todas as 16 cores)
//   Background (fundo): 0-7  (apenas as 8 primeiras, sem bright)
//
// Para montar o atributo VGA:
//   VGA_COLOR(fg, bg)  -> byte de atributo
//   Ex: VGA_COLOR(COLOR_WHITE, COLOR_BLUE)

#ifndef __LEONARDOS_COLORS_H__
#define __LEONARDOS_COLORS_H__

// ============================================================
// Cores base (0-7) - podem ser usadas como fundo e texto
// ============================================================
#define COLOR_BLACK         0x0
#define COLOR_BLUE          0x1
#define COLOR_GREEN         0x2
#define COLOR_CYAN          0x3
#define COLOR_RED           0x4
#define COLOR_MAGENTA       0x5
#define COLOR_BROWN         0x6
#define COLOR_LIGHT_GRAY    0x7

// ============================================================
// Cores claras (8-15) - apenas para texto (foreground)
// ============================================================
#define COLOR_DARK_GRAY     0x8
#define COLOR_LIGHT_BLUE    0x9
#define COLOR_LIGHT_GREEN   0xA
#define COLOR_LIGHT_CYAN    0xB
#define COLOR_LIGHT_RED     0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW        0xE
#define COLOR_WHITE         0xF

// ============================================================
// Macro para montar atributo VGA (byte de cor)
// bg = 4 bits altos, fg = 4 bits baixos
// ============================================================
#define VGA_COLOR(fg, bg) (((bg) << 4) | (fg))

// ============================================================
// Temas predefinidos - combinações prontas para uso
// ============================================================

// Padrão do sistema
#define THEME_DEFAULT       VGA_COLOR(COLOR_LIGHT_GRAY, COLOR_BLACK)

// Shell prompt
#define THEME_PROMPT        VGA_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK)

// Erros
#define THEME_ERROR         VGA_COLOR(COLOR_LIGHT_RED, COLOR_BLACK)

// Avisos
#define THEME_WARNING       VGA_COLOR(COLOR_YELLOW, COLOR_BLACK)

// Sucesso
#define THEME_SUCCESS       VGA_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK)

// Informação / destaque
#define THEME_INFO          VGA_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK)

// Títulos
#define THEME_TITLE         VGA_COLOR(COLOR_WHITE, COLOR_BLACK)

// Texto secundário / desabilitado
#define THEME_DIM           VGA_COLOR(COLOR_DARK_GRAY, COLOR_BLACK)

// Bordas de box-drawing
#define THEME_BORDER        VGA_COLOR(COLOR_BLUE, COLOR_BLACK)

// Labels (sysinfo, etc.)
#define THEME_LABEL         VGA_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK)

// Valores (sysinfo, etc.)
#define THEME_VALUE         VGA_COLOR(COLOR_WHITE, COLOR_BLACK)

// Diretórios (ls, tree, stat)
#define THEME_DIR           VGA_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK)

// Arquivos (ls, tree, stat)
#define THEME_FILE          VGA_COLOR(COLOR_LIGHT_GRAY, COLOR_BLACK)

// Highlight (grep match)
#define THEME_HIGHLIGHT     VGA_COLOR(COLOR_WHITE, COLOR_RED)

// Banner / logo
#define THEME_BANNER        VGA_COLOR(COLOR_WHITE, COLOR_BLACK)

// Kernel boot messages
#define THEME_BOOT          VGA_COLOR(COLOR_DARK_GRAY, COLOR_BLACK)
#define THEME_BOOT_OK       VGA_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK)
#define THEME_BOOT_FAIL     VGA_COLOR(COLOR_LIGHT_RED, COLOR_BLACK)

// Network
#define THEME_NET           VGA_COLOR(COLOR_LIGHT_BLUE, COLOR_BLACK)

#endif
