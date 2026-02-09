// LeonardOS - Comando: stat
// Exibe informações detalhadas de um arquivo ou diretório
//
// Uso:
//   stat /etc/hostname    → info do arquivo
//   stat /mnt             → info do diretório
//   stat arquivo.txt      → relativo ao pwd

#include "cmd_stat.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../fs/leonfs.h"
#include "../shell/shell.h"

void cmd_stat(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: stat <caminho>\n", THEME_DIM);
        return;
    }

    // Pula espaços
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts_color("Uso: stat <caminho>\n", THEME_DIM);
        return;
    }

    // Remove espaços do final
    char clean[256];
    kstrcpy(clean, args, 256);
    {
        int len = kstrlen(clean);
        while (len > 0 && clean[len - 1] == ' ') len--;
        clean[len] = '\0';
    }

    // Resolve path (absoluto ou relativo)
    char resolved[256];
    vfs_node_t *node = vfs_resolve(clean, current_dir, resolved, 256);
    if (!node) {
        vga_puts_color("stat: nao encontrado: ", THEME_ERROR);
        vga_puts_color(clean, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Header
    vga_puts_color("── stat: ", THEME_BORDER);
    vga_puts_color(resolved, THEME_INFO);
    vga_puts_color(" ──\n", THEME_BORDER);

    // Nome
    vga_puts_color("  Nome:       ", THEME_DIM);
    vga_puts_color(node->name, THEME_DEFAULT);
    vga_putchar('\n');

    // Path
    vga_puts_color("  Path:       ", THEME_DIM);
    vga_puts_color(resolved, THEME_INFO);
    vga_putchar('\n');

    // Tipo
    vga_puts_color("  Tipo:       ", THEME_DIM);
    if (node->type & VFS_DIRECTORY) {
        vga_puts_color("diretorio", THEME_DIR);
    } else {
        vga_puts_color("arquivo", THEME_FILE);
    }
    vga_putchar('\n');

    // Tamanho
    vga_puts_color("  Tamanho:    ", THEME_DIM);
    vga_putint(node->size);
    vga_puts_color(" bytes", THEME_DEFAULT);
    vga_putchar('\n');

    // Filesystem
    vga_puts_color("  FS:         ", THEME_DIM);
    if (leonfs_is_node(node)) {
        vga_puts_color("LeonFS (disco)", THEME_INFO);
    } else {
        vga_puts_color("RamFS (RAM)", THEME_INFO);
    }
    vga_putchar('\n');

    // Inode (para LeonFS)
    if (leonfs_is_node(node)) {
        uint32_t inum = (uint32_t)(uintptr_t)node->fs_data;
        vga_puts_color("  Inode:      ", THEME_DIM);
        vga_putint(inum);
        vga_putchar('\n');
    }

    // Para diretórios: conta filhos
    if (node->type & VFS_DIRECTORY) {
        uint32_t count = 0;
        while (vfs_readdir(node, count) != NULL) {
            count++;
        }
        vga_puts_color("  Filhos:     ", THEME_DIM);
        vga_putint(count);
        vga_putchar('\n');
    }

    // Capacidade (para arquivos RamFS)
    if ((node->type & VFS_FILE) && !leonfs_is_node(node)) {
        vga_puts_color("  Max:        ", THEME_DIM);
        vga_putint(RAMFS_MAX_FILE_SIZE);
        vga_puts_color(" bytes", THEME_DEFAULT);
        vga_putchar('\n');
    } else if ((node->type & VFS_FILE) && leonfs_is_node(node)) {
        vga_puts_color("  Max:        ", THEME_DIM);
        vga_putint(LEONFS_MAX_FILE_SIZE);
        vga_puts_color(" bytes", THEME_DEFAULT);
        vga_putchar('\n');
    }
}
