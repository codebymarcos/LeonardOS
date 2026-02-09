// LeonardOS - Comando: grep
// Busca padrão de texto dentro de arquivos
//
// Uso:
//   grep padrão arquivo.txt            → busca "padrão" no arquivo
//   grep padrão /etc/hostname          → caminho absoluto
//   grep -i padrão arquivo.txt         → case-insensitive
//   grep padrão /mnt/                  → busca em todos os arquivos do dir

#include "cmd_grep.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"

// Case-insensitive strstr
static const char *kstrstr_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (needle[0] == '\0') return haystack;

    int nlen = kstrlen(needle);
    int hlen = kstrlen(haystack);
    if (nlen > hlen) return NULL;

    for (int i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            if (ktolower(haystack[i + j]) != ktolower(needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return &haystack[i];
    }
    return NULL;
}

// Busca padrão em um arquivo e exibe linhas com match
static int grep_file(const char *pattern, vfs_node_t *node, const char *display_name,
                     int case_insensitive, int show_filename) {
    if (!node || (node->type & VFS_DIRECTORY) || node->size == 0) return 0;

    // Lê arquivo inteiro em buffer (até 8KB)
    uint8_t buf[8192];
    uint32_t total = node->size;
    if (total > sizeof(buf) - 1) total = sizeof(buf) - 1;

    uint32_t bytes = vfs_read(node, 0, total, buf);
    if (bytes == 0) return 0;
    buf[bytes] = '\0';

    int matches = 0;
    int line_num = 1;

    // Processa linha por linha
    char line[512];
    int li = 0;
    for (uint32_t i = 0; i <= bytes; i++) {
        if (i == bytes || buf[i] == '\n') {
            line[li] = '\0';

            // Busca padrão na linha
            const char *found = case_insensitive
                ? kstrstr_ci(line, pattern)
                : kstrstr(line, pattern);

            if (found) {
                matches++;

                // Prefixo: nome do arquivo (se buscando em diretório)
                if (show_filename) {
                    vga_puts_color(display_name, THEME_INFO);
                    vga_puts_color(":", THEME_DIM);
                }

                // Número da linha
                vga_set_color(THEME_DIM);
                vga_putint(line_num);
                vga_puts_color(": ", THEME_DIM);

                // Exibe a linha com highlight do padrão
                int plen = kstrlen(pattern);
                const char *pos = line;
                while (*pos) {
                    const char *match_pos = case_insensitive
                        ? kstrstr_ci(pos, pattern)
                        : kstrstr(pos, pattern);

                    if (match_pos) {
                        // Texto antes do match
                        while (pos < match_pos) {
                            vga_set_color(THEME_DEFAULT);
                            vga_putchar(*pos++);
                        }
                        // Match em destaque
                        vga_set_color(THEME_HIGHLIGHT);
                        for (int k = 0; k < plen; k++) {
                            vga_putchar(*pos++);
                        }
                    } else {
                        // Resto da linha
                        vga_set_color(THEME_DEFAULT);
                        while (*pos) vga_putchar(*pos++);
                    }
                }
                vga_set_color(THEME_DEFAULT);
                vga_putchar('\n');
            }

            line_num++;
            li = 0;
        } else if (li < 510) {
            line[li++] = (char)buf[i];
        }
    }

    return matches;
}

void cmd_grep(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: grep [-i] <padrao> <arquivo|dir>\n", THEME_DIM);
        return;
    }

    while (*args == ' ') args++;

    // Verifica flag -i
    int case_insensitive = 0;
    if (args[0] == '-' && args[1] == 'i' && (args[2] == ' ' || args[2] == '\0')) {
        case_insensitive = 1;
        args += 2;
        while (*args == ' ') args++;
    }

    if (*args == '\0') {
        vga_puts_color("Uso: grep [-i] <padrao> <arquivo|dir>\n", THEME_DIM);
        return;
    }

    // Extrai padrão
    char pattern[128];
    int pi = 0;

    // Suporte a padrão entre aspas
    if (*args == '"') {
        args++; // pula abertura
        while (*args && *args != '"' && pi < 127) {
            pattern[pi++] = *args++;
        }
        if (*args == '"') args++; // pula fechamento
    } else {
        while (*args && *args != ' ' && pi < 127) {
            pattern[pi++] = *args++;
        }
    }
    pattern[pi] = '\0';

    if (pattern[0] == '\0') {
        vga_puts_color("grep: padrao vazio\n", THEME_ERROR);
        return;
    }

    // Extrai path
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts_color("Uso: grep [-i] <padrao> <arquivo|dir>\n", THEME_DIM);
        return;
    }

    char path_arg[256];
    int di = 0;
    while (*args && *args != ' ' && di < 255) {
        path_arg[di++] = *args++;
    }
    path_arg[di] = '\0';

    // Resolve path
    char resolved[256];
    vfs_node_t *node = vfs_resolve(path_arg, current_dir, resolved, 256);
    if (!node) {
        vga_puts_color("grep: nao encontrado: ", THEME_ERROR);
        vga_puts_color(path_arg, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    int total_matches = 0;

    if (node->type & VFS_DIRECTORY) {
        // Busca em todos os arquivos do diretório
        uint32_t idx = 0;
        vfs_node_t *child;
        while ((child = vfs_readdir(node, idx)) != NULL) {
            if (child->type & VFS_FILE) {
                // Constrói nome para exibição
                char display[320];
                kstrcpy(display, resolved, 256);
                int rlen = kstrlen(display);
                if (rlen > 0 && display[rlen - 1] != '/') {
                    kstrcat(display, "/", 320);
                }
                kstrcat(display, child->name, 320);

                total_matches += grep_file(pattern, child, display,
                                           case_insensitive, 1);
            }
            idx++;
        }
    } else {
        total_matches = grep_file(pattern, node, resolved,
                                  case_insensitive, 0);
    }

    // Resumo
    if (total_matches == 0) {
        vga_puts_color("Nenhum match para '", THEME_DIM);
        vga_puts_color(pattern, THEME_WARNING);
        vga_puts_color("'\n", THEME_DIM);
    } else {
        vga_putchar('\n');
        vga_putint(total_matches);
        vga_puts_color(" match(es) encontrado(s)\n", THEME_DIM);
    }
}
