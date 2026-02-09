// LeonardOS - LeonFS (Filesystem persistente em disco)
// Filesystem próprio do LeonardOS para discos ATA
//
// Layout do disco:
//   Setor 0       : Superbloco (512 bytes)
//   Setores 1-8   : Bitmap de blocos livres (8 setores = 4096 bytes = 32768 bits = 32768 blocos)
//   Setores 9-72  : Tabela de inodes (64 setores = 512 inodes de 64 bytes cada)
//   Setor 73+     : Blocos de dados (1 bloco = 1 setor = 512 bytes)
//
// Cada inode aponta para até 10 blocos diretos (5120 bytes por arquivo).
// Inodes são indexados de 0 a 511. Inode 0 = diretório raiz.
//
// Diretórios: conteúdo é uma lista de dir_entry_t (nome + inode_num).

#ifndef __LEONFS_H__
#define __LEONFS_H__

#include "../common/types.h"
#include "vfs.h"

// ============================================================
// Constantes do LeonFS
// ============================================================

#define LEONFS_MAGIC         0x4C454F4E  // "LEON" em ASCII

#define LEONFS_BLOCK_SIZE    512         // 1 bloco = 1 setor

// Layout em setores
#define LEONFS_SUPERBLOCK_SECTOR  0
#define LEONFS_BITMAP_START       1      // Bitmap: setores 1-8
#define LEONFS_BITMAP_SECTORS     8
#define LEONFS_INODE_START        9      // Inodes: setores 9-72
#define LEONFS_INODE_SECTORS      64
#define LEONFS_DATA_START         73     // Dados: setor 73+

// Limites
#define LEONFS_MAX_INODES        512     // 64 setores * 8 inodes/setor
#define LEONFS_MAX_BLOCKS        32768   // 8 setores bitmap * 512 bytes * 8 bits
#define LEONFS_DIRECT_BLOCKS     10      // Blocos diretos por inode
#define LEONFS_MAX_FILE_SIZE     (LEONFS_DIRECT_BLOCKS * LEONFS_BLOCK_SIZE)  // 5120 bytes
#define LEONFS_MAX_NAME          60      // Nome máximo em dir_entry

// Tipos de inode
#define LEONFS_TYPE_FREE         0
#define LEONFS_TYPE_FILE         1
#define LEONFS_TYPE_DIR          2

// ============================================================
// Estruturas on-disk (packed, layout exato em disco)
// ============================================================

// Superbloco — 512 bytes (setor 0)
typedef struct __attribute__((packed)) {
    uint32_t magic;             // LEONFS_MAGIC
    uint32_t version;           // Versão do formato (1)
    uint32_t total_blocks;      // Total de blocos de dados
    uint32_t free_blocks;       // Blocos livres
    uint32_t total_inodes;      // Total de inodes
    uint32_t free_inodes;       // Inodes livres
    uint32_t root_inode;        // Inode do diretório raiz (sempre 0)
    uint8_t  _reserved[512 - 28]; // Padding até 512 bytes
} leonfs_superblock_t;

// Inode — 64 bytes
typedef struct __attribute__((packed)) {
    uint8_t  type;              // LEONFS_TYPE_*
    uint8_t  _pad1;
    uint16_t _pad2;
    uint32_t size;              // Tamanho em bytes
    uint32_t blocks[LEONFS_DIRECT_BLOCKS]; // Números de bloco (setor absoluto)
    uint8_t  _reserved[64 - 48]; // Padding até 64 bytes
} leonfs_inode_t;

// Entrada de diretório — 64 bytes
typedef struct __attribute__((packed)) {
    uint32_t inode_num;         // Número do inode
    char     name[LEONFS_MAX_NAME]; // Nome do arquivo/diretório
} leonfs_dir_entry_t;

// Entradas por bloco de diretório
#define LEONFS_DIR_ENTRIES_PER_BLOCK (LEONFS_BLOCK_SIZE / sizeof(leonfs_dir_entry_t))

// ============================================================
// API pública
// ============================================================

// Inicializa LeonFS — lê superbloco do disco
// Se disco não tem LeonFS, formata automaticamente
// Retorna nó VFS raiz do LeonFS, ou NULL se falhar
vfs_node_t *leonfs_init(void);

// Formata o disco com LeonFS vazio
// Cria superbloco, bitmap, inode root
// Retorna true se sucesso
bool leonfs_format(void);

// Cria um arquivo em um diretório LeonFS
// Retorna nó VFS do arquivo criado, ou NULL se falhar
vfs_node_t *leonfs_create_file(vfs_node_t *parent, const char *name);

// Cria um diretório em um diretório LeonFS
// Retorna nó VFS do diretório criado, ou NULL se falhar
vfs_node_t *leonfs_create_dir(vfs_node_t *parent, const char *name);

// Verifica se um nó VFS pertence ao LeonFS
bool leonfs_is_node(vfs_node_t *node);

// Remove um arquivo ou diretório vazio de um diretório LeonFS
// Retorna true se removido com sucesso
bool leonfs_remove(vfs_node_t *parent, const char *name);

#endif
