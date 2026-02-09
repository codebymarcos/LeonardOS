// Shell - Interface pública

#ifndef __SHELL_H__
#define __SHELL_H__

#include "../fs/vfs.h"

// Estado global do shell
extern vfs_node_t *current_dir;    // Diretório atual
extern char current_path[256];      // Path textual do diretório atual

// Loop principal
void shell_loop(void);

// Executa uma linha de comando (com suporte a ;, |, $VAR, glob)
void execute_line(const char *input);

// Último código de saída (0 = sucesso)
extern int last_exit_code;

// Variáveis de ambiente
void shell_setenv(const char *key, const char *value);
const char *shell_getenv(const char *key);

#endif
