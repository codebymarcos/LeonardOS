// LeonardOS - VFS (Virtual File System)
// Camada de abstração para sistemas de arquivo
//
// Desacopla shell ↔ filesystem. O VFS não sabe
// como o backend funciona — delega via function pointers.
// API: vfs_init, vfs_open, vfs_read, vfs_write, vfs_close

#ifndef __VFS_H__
#define __VFS_H__

#include "../common/types.h"

// ============================================================
// Tipos de nó
// ============================================================
#define VFS_FILE      0x01
#define VFS_DIRECTORY 0x02

// ============================================================
// Estrutura de nó VFS
// ============================================================
// Cada arquivo ou diretório é representado por um vfs_node.
// Os function pointers são preenchidos pelo backend (RamFS, etc).

typedef struct vfs_node {
    char name[64];              // Nome do arquivo/diretório
    uint32_t type;              // VFS_FILE ou VFS_DIRECTORY
    uint32_t size;              // Tamanho em bytes (para arquivos)

    // Operações do backend (preenchidas pelo FS concreto)
    uint32_t (*read)(struct vfs_node *, uint32_t offset, uint32_t size, uint8_t *buffer);
    uint32_t (*write)(struct vfs_node *, uint32_t offset, uint32_t size, const uint8_t *buffer);
    void     (*open)(struct vfs_node *);
    void     (*close)(struct vfs_node *);

    // Operações de diretório
    struct vfs_node *(*readdir)(struct vfs_node *, uint32_t index);
    struct vfs_node *(*finddir)(struct vfs_node *, const char *name);

    void *fs_data;              // Dados privados do backend (VFS não interpreta)
} vfs_node_t;

// ============================================================
// Raiz do VFS
// ============================================================
extern vfs_node_t *vfs_root;

// ============================================================
// API pública
// ============================================================

// Inicializa o VFS (define raiz como NULL)
void vfs_init(void);

// Monta um nó como raiz do VFS
void vfs_mount_root(vfs_node_t *root);

// Resolve um path e retorna o nó correspondente
// Path deve começar com '/' (absoluto)
// Retorna NULL se não encontrar
vfs_node_t *vfs_open(const char *path);

// Resolve path absoluto OU relativo a partir de base_dir
// Suporta ".", "..", e caminhos multi-nível (ex: "../tmp/file")
// Se path começa com '/', resolve como absoluto (ignora base_dir)
// Retorna NULL se não encontrar
// Também preenche resolved_path (se não-NULL) com o path absoluto canônico
vfs_node_t *vfs_resolve(const char *path, vfs_node_t *base_dir,
                         char *resolved_path, int resolved_max);

// Constrói path absoluto canônico a partir de base + relative
// Resolve ".", ".." e barras duplicadas
// Resultado vai em out_path (até max chars)
// Retorna true se o path resultante é válido
bool vfs_build_path(const char *base_path, const char *relative,
                     char *out_path, int max);

// Lê bytes de um nó (arquivo)
// Retorna quantidade de bytes lidos
uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);

// Escreve bytes em um nó (arquivo)
// Retorna quantidade de bytes escritos
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);

// Fecha um nó
void vfs_close(vfs_node_t *node);

// Lista entrada de diretório pelo índice
// Retorna NULL quando index está fora do range
vfs_node_t *vfs_readdir(vfs_node_t *dir, uint32_t index);

// Busca entrada em diretório pelo nome
vfs_node_t *vfs_finddir(vfs_node_t *dir, const char *name);

#endif
