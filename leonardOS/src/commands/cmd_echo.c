// LeonardOS - Comando: echo
// Escreve texto em arquivo ou exibe no terminal via VFS
//
// Uso:
//   echo hello                → exibe "hello" no terminal
//   echo hello > /tmp/test    → escreve em /tmp/test (absoluto)
//   echo hello > test.txt     → escreve em test.txt (relativo ao pwd)
//   echo hello > ../tmp/test  → relativo com ..

#include "cmd_echo.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../shell/shell.h"

void cmd_echo(const char *args) {
    if (!args || args[0] == '\0') {
        vga_putchar('\n');
        return;
    }

    // Procura '>' no args para redirecionamento
    const char *redir = NULL;
    const char *p = args;
    while (*p) {
        if (*p == '>') {
            redir = p;
            break;
        }
        p++;
    }

    if (!redir) {
        // Sem redirecionamento: exibe no terminal
        vga_puts(args);
        vga_putchar('\n');
        return;
    }

    // Extrai o texto antes do '>'
    char text[256];
    int text_len = 0;
    p = args;
    while (p < redir && text_len < 255) {
        text[text_len++] = *p++;
    }
    // Remove espaços no final
    while (text_len > 0 && text[text_len - 1] == ' ') text_len--;
    text[text_len] = '\0';

    // Extrai o path após '>'
    const char *path = redir + 1;
    while (*path == ' ') path++;

    if (*path == '\0') {
        vga_puts_color("echo: caminho de arquivo ausente\n", THEME_ERROR);
        return;
    }

    // Remove espaços do final do path
    char clean_path[256];
    kstrcpy(clean_path, path, 256);
    {
        int plen = kstrlen(clean_path);
        while (plen > 0 && clean_path[plen - 1] == ' ') plen--;
        clean_path[plen] = '\0';
    }

    // Separa diretório pai e nome do arquivo
    // Encontra último '/'
    int path_len = kstrlen(clean_path);
    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (clean_path[i] == '/') last_slash = i;
    }

    char dir_path[256];
    char file_name[64];

    if (clean_path[0] == '/' && last_slash == 0) {
        // Arquivo na raiz: /test.txt
        kstrcpy(dir_path, "/", 256);
        kstrcpy(file_name, clean_path + 1, 64);
    } else if (last_slash > 0) {
        // Caminho com diretório: /tmp/test.txt ou ../tmp/test.txt
        // dir_path = tudo antes do último '/'
        int di = 0;
        for (int i = 0; i < last_slash && di < 255; i++) {
            dir_path[di++] = clean_path[i];
        }
        dir_path[di] = '\0';

        // file_name = tudo após o último '/'
        kstrcpy(file_name, clean_path + last_slash + 1, 64);
    } else {
        // Sem '/' — arquivo no diretório atual
        kstrcpy(dir_path, current_path, 256);
        kstrcpy(file_name, clean_path, 64);
    }

    if (file_name[0] == '\0') {
        vga_puts_color("echo: nome de arquivo invalido\n", THEME_ERROR);
        return;
    }

    // Resolve o diretório pai (absoluto ou relativo)
    vfs_node_t *parent = vfs_resolve(dir_path, current_dir, NULL, 0);
    if (!parent || !(parent->type & VFS_DIRECTORY)) {
        vga_puts_color("echo: diretorio nao encontrado: ", THEME_ERROR);
        vga_puts_color(dir_path, THEME_WARNING);
        vga_putchar('\n');
        return;
    }

    // Cria ou reutiliza o arquivo
    vfs_node_t *file = ramfs_create_file(parent, file_name);
    if (!file) {
        vga_puts_color("echo: nao foi possivel criar arquivo\n", THEME_ERROR);
        return;
    }

    // Reseta tamanho para overwrite
    file->size = 0;

    // Escreve o conteúdo
    if (text_len > 0) {
        uint32_t written = vfs_write(file, 0, (uint32_t)text_len, (const uint8_t *)text);
        if (written == 0) {
            vga_puts_color("echo: erro ao escrever\n", THEME_ERROR);
            return;
        }
    }

    // Feedback
    vga_puts_color("Escrito ", THEME_DIM);
    vga_putint(text_len);
    vga_puts_color(" bytes em ", THEME_DIM);
    vga_puts_color(clean_path, THEME_INFO);
    vga_putchar('\n');
}
