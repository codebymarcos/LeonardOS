// Shell - Interface pública

#ifndef __SHELL_H__
#define __SHELL_H__

#include "../fs/vfs.h"

// Estado global do shell
extern vfs_node_t *current_dir;    // Diretório atual
extern char current_path[256];      // Path textual do diretório atual

void shell_loop(void);

#endif
