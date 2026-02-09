// LeonardOS - RamFS (Filesystem em RAM)
// Backend do VFS para arquivos e diretórios em memória
//
// Usa kmalloc/kfree para alocação.
// Cada nó RamFS tem dados privados (ramfs_data_t) acessíveis via fs_data.
// API: ramfs_init (retorna raiz montável no VFS)

#ifndef __RAMFS_H__
#define __RAMFS_H__

#include "vfs.h"

// ============================================================
// Limites do RamFS
// ============================================================

// Máximo de filhos por diretório
#define RAMFS_MAX_CHILDREN  32

// Tamanho máximo de conteúdo de arquivo
#define RAMFS_MAX_FILE_SIZE 4096

// Máximo de nós alocáveis (pool estático para evitar fragmentação)
#define RAMFS_MAX_NODES     64

// ============================================================
// Dados privados de cada nó RamFS
// ============================================================
typedef struct {
    // Para arquivos: buffer de dados
    uint8_t *data;              // kmalloc'd buffer (ou NULL)
    uint32_t capacity;          // Tamanho alocado

    // Para diretórios: lista de filhos
    vfs_node_t *children[RAMFS_MAX_CHILDREN];
    uint32_t child_count;

    // Referência ao pai
    vfs_node_t *parent;
} ramfs_data_t;

// ============================================================
// API pública
// ============================================================

// Inicializa o RamFS e retorna o nó raiz "/"
// Cria estrutura inicial com diretórios padrão
vfs_node_t *ramfs_init(void);

// Cria um arquivo dentro de um diretório
// Retorna o nó criado, ou NULL se falhar
vfs_node_t *ramfs_create_file(vfs_node_t *parent, const char *name);

// Cria um subdiretório dentro de um diretório
// Retorna o nó criado, ou NULL se falhar
vfs_node_t *ramfs_create_dir(vfs_node_t *parent, const char *name);

#endif
