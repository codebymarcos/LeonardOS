// LeonardOS - Comando: find
// Busca arquivos e diretórios por nome (substring match)
//
// Uso:
//   find hostname          → busca "hostname" a partir do pwd
//   find hostname /        → busca "hostname" a partir da raiz
//   find .txt /mnt         → busca tudo com ".txt" em /mnt

#include "cmd_find.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"

// Profundidade máxima (mesmo limite do tree)
#define FIND_MAX_DEPTH 8

static int find_count;

// Substring match simples
static bool contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    int hlen = kstrlen(haystack);
    int nlen = kstrlen(needle);
    if (nlen == 0) return true;
    if (nlen > hlen) return false;

    for (int i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (int j = 0; j < nlen; j++) {
            if (haystack[i + j] != needle[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static void find_recurse(vfs_node_t *node, const char *pattern,
                          char *path_buf, int path_len, int depth) {
    if (depth >= FIND_MAX_DEPTH) return;

    uint32_t idx = 0;
    vfs_node_t *child;

    while ((child = vfs_readdir(node, idx)) != NULL) {
        // Constrói path deste filho
        char child_path[256];
        int cp = 0;

        // Copia path_buf
        for (int i = 0; i < path_len && cp < 254; i++) {
            child_path[cp++] = path_buf[i];
        }

        // Adiciona '/' se necessário
        if (cp > 0 && child_path[cp - 1] != '/' && cp < 254) {
            child_path[cp++] = '/';
        }

        // Adiciona nome do filho
        const char *name = child->name;
        int ni = 0;
        while (name[ni] && cp < 254) {
            child_path[cp++] = name[ni++];
        }
        child_path[cp] = '\0';

        // Verifica match no nome
        if (contains(child->name, pattern)) {
            if (child->type & VFS_DIRECTORY) {
                vga_puts_color(child_path, THEME_DIR);
            } else {
                vga_puts_color(child_path, THEME_FILE);
            }
            vga_putchar('\n');
            find_count++;
        }

        // Recurse em diretórios
        if (child->type & VFS_DIRECTORY) {
            find_recurse(child, pattern, child_path, cp, depth + 1);
        }

        idx++;
    }
}

void cmd_find(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: find <padrao> [caminho]\n", THEME_DIM);
        return;
    }

    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts_color("Uso: find <padrao> [caminho]\n", THEME_DIM);
        return;
    }

    // Extrai padrão
    char pattern[64];
    int pi = 0;
    while (*args && *args != ' ' && pi < 63) {
        pattern[pi++] = *args++;
    }
    pattern[pi] = '\0';

    // Extrai caminho (opcional)
    while (*args == ' ') args++;

    vfs_node_t *start;
    char start_path[256];

    if (*args != '\0') {
        // Caminho fornecido
        char path_arg[256];
        int di = 0;
        while (*args && *args != ' ' && di < 255) {
            path_arg[di++] = *args++;
        }
        path_arg[di] = '\0';

        start = vfs_resolve(path_arg, current_dir, start_path, 256);
        if (!start) {
            vga_puts_color("find: caminho nao encontrado: ", THEME_ERROR);
            vga_puts_color(path_arg, THEME_WARNING);
            vga_putchar('\n');
            return;
        }
        if (!(start->type & VFS_DIRECTORY)) {
            vga_puts_color("find: nao e diretorio: ", THEME_ERROR);
            vga_puts_color(path_arg, THEME_WARNING);
            vga_putchar('\n');
            return;
        }
    } else {
        // Usa diretório atual
        start = current_dir;
        kstrcpy(start_path, current_path, 256);
    }

    find_count = 0;

    // Busca recursiva
    int slen = kstrlen(start_path);
    find_recurse(start, pattern, start_path, slen, 0);

    // Resumo
    if (find_count == 0) {
        vga_puts_color("Nenhum resultado para '", THEME_DIM);
        vga_puts_color(pattern, THEME_WARNING);
        vga_puts_color("'\n", THEME_DIM);
    } else {
        vga_putint(find_count);
        vga_puts_color(" encontrado(s)\n", THEME_DIM);
    }
}
