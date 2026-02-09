// LeonardOS - Comando: cp
// Copia um arquivo para outro destino
//
// Uso:
//   cp /etc/hostname /tmp/copia     → copia absoluto
//   cp hostname ../tmp/copia        → copia relativo
//   cp /etc/hostname /tmp/          → copia para diretório (mantém nome)
//
// Apenas copia arquivos (não diretórios).

#include "cmd_cp.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../shell/shell.h"

void cmd_cp(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: cp <origem> <destino>\n", THEME_DIM);
        return;
    }

    // Pula espaços
    while (*args == ' ') args++;

    // Extrai primeiro argumento (origem)
    char src_arg[256];
    int si = 0;
    while (*args && *args != ' ' && si < 255) {
        src_arg[si++] = *args++;
    }
    src_arg[si] = '\0';

    // Pula espaços entre argumentos
    while (*args == ' ') args++;

    if (*args == '\0') {
        vga_puts_color("Uso: cp <origem> <destino>\n", THEME_DIM);
        return;
    }

    // Extrai segundo argumento (destino)
    char dst_arg[256];
    int di = 0;
    while (*args && *args != ' ' && di < 255) {
        dst_arg[di++] = *args++;
    }
    dst_arg[di] = '\0';

    // Resolve path da origem
    char src_path[256];
    vfs_node_t *src = vfs_resolve(src_arg, current_dir, src_path, 256);
    if (!src) {
        vga_puts_color("cp: origem nao encontrada: ", THEME_ERROR);
        vga_puts_color(src_arg, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    if (src->type & VFS_DIRECTORY) {
        vga_puts_color("cp: nao copia diretorios: ", THEME_ERROR);
        vga_puts_color(src_arg, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Resolve destino
    char dst_full[256];
    if (!vfs_build_path(current_path, dst_arg, dst_full, 256)) {
        vga_puts_color("cp: caminho destino invalido\n", THEME_ERROR);
        return;
    }

    // Verifica se destino é um diretório existente
    // Se for, copia para dentro com o mesmo nome
    vfs_node_t *dst_check = vfs_open(dst_full);
    char parent_path[256];
    char file_name[64];

    if (dst_check && (dst_check->type & VFS_DIRECTORY)) {
        // Destino é diretório — copia pra dentro com nome original
        kstrcpy(parent_path, dst_full, 256);
        kstrcpy(file_name, src->name, 64);
    } else {
        // Destino é um path de arquivo — separa pai e nome
        int path_len = kstrlen(dst_full);
        int last_slash = -1;
        for (int i = 0; i < path_len; i++) {
            if (dst_full[i] == '/') last_slash = i;
        }

        if (last_slash <= 0) {
            kstrcpy(parent_path, "/", 256);
            kstrcpy(file_name, dst_full + 1, 64);
        } else {
            int pi = 0;
            for (int i = 0; i < last_slash && pi < 255; i++) {
                parent_path[pi++] = dst_full[i];
            }
            parent_path[pi] = '\0';
            kstrcpy(file_name, dst_full + last_slash + 1, 64);
        }
    }

    if (file_name[0] == '\0') {
        vga_puts_color("cp: nome de destino invalido\n", THEME_ERROR);
        return;
    }

    // Resolve diretório pai do destino
    vfs_node_t *parent = vfs_open(parent_path);
    if (!parent || !(parent->type & VFS_DIRECTORY)) {
        vga_puts_color("cp: diretorio destino nao encontrado: ", THEME_ERROR);
        vga_puts_color(parent_path, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Cria arquivo destino
    vfs_node_t *dst = ramfs_create_file(parent, file_name);
    if (!dst) {
        vga_puts_color("cp: falha ao criar destino\n", THEME_ERROR);
        return;
    }

    // Reseta destino para overwrite
    dst->size = 0;

    // Copia conteúdo em chunks
    if (src->size > 0) {
        uint8_t buf[512];
        uint32_t offset = 0;
        uint32_t total = 0;

        while (offset < src->size) {
            uint32_t chunk = src->size - offset;
            if (chunk > sizeof(buf)) chunk = sizeof(buf);

            uint32_t rd = vfs_read(src, offset, chunk, buf);
            if (rd == 0) break;

            uint32_t wr = vfs_write(dst, offset, rd, buf);
            if (wr == 0) break;

            total += wr;
            offset += rd;
        }

        vga_puts_color("Copiado ", THEME_DIM);
        vga_putint(total);
        vga_puts_color(" bytes: ", THEME_DIM);
    } else {
        vga_puts_color("Copiado: ", THEME_DIM);
    }

    vga_puts_color(src_arg, THEME_INFO);
    vga_puts_color(" -> ", THEME_DIM);
    vga_puts_color(file_name, THEME_INFO);
    vga_putchar('\n');
}
