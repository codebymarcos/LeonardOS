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
#include "../fs/leonfs.h"
#include "../shell/shell.h"

void cmd_echo(const char *args) {
    if (!args || args[0] == '\0') {
        vga_putchar('\n');
        return;
    }

    // Procura '>' ou '>>' no args para redirecionamento (respeitando aspas)
    const char *redir = NULL;
    int append_mode = 0;  // 1 = '>>' (append), 0 = '>' (overwrite)
    const char *p = args;
    int in_quotes = 0;
    while (*p) {
        if (*p == '"' && (p == args || *(p - 1) != '\\')) {
            in_quotes = !in_quotes;
        } else if (*p == '>' && !in_quotes) {
            redir = p;
            if (*(p + 1) == '>') {
                append_mode = 1;
            }
            break;
        }
        p++;
    }

    if (!redir) {
        // Sem redirecionamento: exibe no terminal (processando aspas e escapes)
        p = args;
        while (*p) {
            if (*p == '"') {
                p++; // pula aspas
            } else if (*p == '\\' && *(p + 1)) {
                p++; // pula backslash
                if (*p == 'n') vga_putchar('\n');
                else if (*p == 't') vga_putchar('\t');
                else if (*p == '\\') vga_putchar('\\');
                else if (*p == '"') vga_putchar('"');
                else { vga_putchar('\\'); vga_putchar(*p); }
                p++;
            } else {
                vga_putchar(*p++);
            }
        }
        vga_putchar('\n');
        return;
    }

    // Extrai o texto antes do '>' (processando aspas e escapes)
    char text[256];
    int text_len = 0;
    p = args;
    while (p < redir && text_len < 255) {
        if (*p == '"') {
            p++; // pula aspas
        } else if (*p == '\\' && *(p + 1) && p + 1 < redir) {
            p++; // pula backslash
            if (*p == 'n') text[text_len++] = '\n';
            else if (*p == 't') text[text_len++] = '\t';
            else if (*p == '\\') text[text_len++] = '\\';
            else if (*p == '"') text[text_len++] = '"';
            else { text[text_len++] = *p; }
            p++;
        } else {
            text[text_len++] = *p++;
        }
    }
    // Remove espaços no final
    while (text_len > 0 && text[text_len - 1] == ' ') text_len--;
    text[text_len] = '\0';

    // Extrai o path após '>' ou '>>'
    const char *path = redir + 1;
    if (append_mode) path++;  // pula o segundo '>'
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

    // Cria ou reutiliza o arquivo (detecta FS correto)
    vfs_node_t *file;
    if (leonfs_is_node(parent)) {
        file = leonfs_create_file(parent, file_name);
    } else {
        file = ramfs_create_file(parent, file_name);
    }
    if (!file) {
        vga_puts_color("echo: nao foi possivel criar arquivo\n", THEME_ERROR);
        return;
    }

    // Reseta tamanho para overwrite, ou mantém para append
    uint32_t write_offset = 0;
    if (append_mode) {
        write_offset = file->size;
    } else {
        file->size = 0;
    }

    // Escreve o conteúdo
    if (text_len > 0) {
        uint32_t written = vfs_write(file, write_offset, (uint32_t)text_len, (const uint8_t *)text);
        if (written == 0) {
            vga_puts_color("echo: erro ao escrever\n", THEME_ERROR);
            return;
        }
    }

    // Feedback
    const char *mode_str = append_mode ? "Adicionado " : "Escrito ";
    vga_puts_color(mode_str, THEME_DIM);
    vga_putint(text_len);
    vga_puts_color(" bytes em ", THEME_DIM);
    vga_puts_color(clean_path, THEME_INFO);
    vga_putchar('\n');
}
