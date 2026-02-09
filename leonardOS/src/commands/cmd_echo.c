// LeonardOS - Comando: echo
// Escreve texto em arquivo ou exibe no terminal via VFS
//
// Uso:
//   echo hello              → exibe "hello" no terminal
//   echo hello > /tmp/test  → escreve "hello" em /tmp/test (cria se não existe)

#include "cmd_echo.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"

// Helpers
static int echo_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

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
    // Copia texto (sem trailing spaces)
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

    // Resolve o diretório pai e nome do arquivo
    // Encontra o último '/'
    char dir_path[256];
    char file_name[64];
    int path_len = echo_strlen(path);

    // Encontra último '/'
    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash <= 0) {
        // Arquivo na raiz: /test.txt
        dir_path[0] = '/';
        dir_path[1] = '\0';
        // Nome começa após o último '/'
        int ni = 0;
        int start = (last_slash >= 0) ? last_slash + 1 : 0;
        for (int i = start; i < path_len && ni < 63; i++) {
            file_name[ni++] = path[i];
        }
        file_name[ni] = '\0';
    } else {
        // Diretório + arquivo
        int di = 0;
        for (int i = 0; i < last_slash && di < 255; i++) {
            dir_path[di++] = path[i];
        }
        dir_path[di] = '\0';

        int ni = 0;
        for (int i = last_slash + 1; i < path_len && ni < 63; i++) {
            file_name[ni++] = path[i];
        }
        file_name[ni] = '\0';
    }

    // Remove espaços do nome do arquivo
    {
        int ni = echo_strlen(file_name);
        while (ni > 0 && file_name[ni - 1] == ' ') ni--;
        file_name[ni] = '\0';
    }

    if (file_name[0] == '\0') {
        vga_puts_color("echo: nome de arquivo invalido\n", THEME_ERROR);
        return;
    }

    // Abre o diretório pai
    vfs_node_t *parent = vfs_open(dir_path);
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
    vga_puts_color(path, THEME_INFO);
    vga_putchar('\n');
}
