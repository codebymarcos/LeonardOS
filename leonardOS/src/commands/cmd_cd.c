// LeonardOS - Comando: cd
// Muda de diretório
//
// Uso:
//   cd           → volta para raiz (/)
//   cd /etc      → caminho absoluto
//   cd tmp       → relativo ao pwd
//   cd ..        → diretório pai
//   cd ../tmp    → relativo com ..

#include "cmd_cd.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../shell/shell.h"
#include "../fs/vfs.h"

void cmd_cd(const char *args) {
    // cd sem argumento → volta para raiz
    if (!args || args[0] == '\0') {
        current_dir = vfs_root;
        kstrcpy(current_path, "/", 256);
        return;
    }

    // Pula espaços
    while (*args == ' ') args++;
    if (*args == '\0') {
        current_dir = vfs_root;
        kstrcpy(current_path, "/", 256);
        return;
    }

    // Resolve via VFS (absoluto ou relativo, com suporte a "..")
    char resolved[256];
    vfs_node_t *target = vfs_resolve(args, current_dir, resolved, 256);

    if (!target) {
        vga_puts_color("cd: nao encontrado: ", THEME_ERROR);
        vga_puts_color(args, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    if (!(target->type & VFS_DIRECTORY)) {
        vga_puts_color("cd: nao e um diretorio: ", THEME_ERROR);
        vga_puts_color(args, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Atualiza estado global
    current_dir = target;
    kstrcpy(current_path, resolved, 256);
}
