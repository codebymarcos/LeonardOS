// LeonardOS - VFS (Virtual File System)
// Implementação: path parsing + dispatch para backend

#include "vfs.h"

// ============================================================
// Raiz global do VFS
// ============================================================
vfs_node_t *vfs_root = NULL;

// ============================================================
// vfs_init — Inicializa o VFS
// ============================================================
void vfs_init(void) {
    vfs_root = NULL;
}

// ============================================================
// vfs_mount_root — Define o nó raiz
// ============================================================
void vfs_mount_root(vfs_node_t *root) {
    vfs_root = root;
}

// ============================================================
// Helpers internos
// ============================================================

// ============================================================
// vfs_open — Resolve path absoluto para um nó
// ============================================================
// Parsing simples: split por '/', traversal via finddir
// Sem suporte a ".." ou "." por enquanto
vfs_node_t *vfs_open(const char *path) {
    if (!path || !vfs_root) return NULL;

    // Path vazio ou apenas espaços
    while (*path == ' ') path++;
    if (*path == '\0') return NULL;

    // "/" sozinho → retorna raiz
    if (path[0] == '/' && path[1] == '\0') {
        if (vfs_root->open) vfs_root->open(vfs_root);
        return vfs_root;
    }

    // Deve começar com '/'
    if (path[0] != '/') return NULL;
    path++;  // pula o '/' inicial

    vfs_node_t *current = vfs_root;

    // Itera pelos componentes do path
    while (*path) {
        // Pula barras extras
        while (*path == '/') path++;
        if (*path == '\0') break;

        // Extrai próximo componente
        char component[64];
        int i = 0;
        while (*path && *path != '/' && i < 63) {
            component[i++] = *path++;
        }
        component[i] = '\0';

        // current deve ser um diretório para continuar o traversal
        if (!(current->type & VFS_DIRECTORY)) return NULL;
        if (!current->finddir) return NULL;

        vfs_node_t *next = current->finddir(current, component);
        if (!next) return NULL;

        current = next;
    }

    // Chama open se disponível
    if (current->open) current->open(current);

    return current;
}

// ============================================================
// vfs_read — Lê de um nó
// ============================================================
uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node || !node->read || !buffer) return 0;
    return node->read(node, offset, size, buffer);
}

// ============================================================
// vfs_write — Escreve em um nó
// ============================================================
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    if (!node || !node->write || !buffer) return 0;
    return node->write(node, offset, size, buffer);
}

// ============================================================
// vfs_close — Fecha um nó
// ============================================================
void vfs_close(vfs_node_t *node) {
    if (node && node->close) node->close(node);
}

// ============================================================
// vfs_readdir — Lista entrada de diretório por índice
// ============================================================
vfs_node_t *vfs_readdir(vfs_node_t *dir, uint32_t index) {
    if (!dir || !(dir->type & VFS_DIRECTORY) || !dir->readdir) return NULL;
    return dir->readdir(dir, index);
}

// ============================================================
// vfs_finddir — Busca em diretório por nome
// ============================================================
vfs_node_t *vfs_finddir(vfs_node_t *dir, const char *name) {
    if (!dir || !(dir->type & VFS_DIRECTORY) || !dir->finddir || !name) return NULL;
    return dir->finddir(dir, name);
}
