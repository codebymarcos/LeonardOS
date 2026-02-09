// LeonardOS - VFS (Virtual File System)
// Implementação: path parsing + dispatch para backend

#include "vfs.h"
#include "../common/string.h"

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
// vfs_build_path — Constrói path absoluto canônico
// ============================================================
// Resolve ".", "..", barras duplicadas.
// base_path deve ser absoluto (começa com '/').
// relative pode ser absoluto (ignora base) ou relativo.
bool vfs_build_path(const char *base_path, const char *relative,
                     char *out_path, int max) {
    if (!out_path || max < 2) return false;

    // Se relative é absoluto, usa direto
    char work[256];

    if (relative && relative[0] == '/') {
        kstrcpy(work, relative, 256);
    } else {
        // base + "/" + relative
        if (!base_path) return false;
        kstrcpy(work, base_path, 256);
        if (relative && relative[0] != '\0') {
            int blen = kstrlen(work);
            if (blen > 0 && work[blen - 1] != '/') {
                kstrcat(work, "/", 256);
            }
            kstrcat(work, relative, 256);
        }
    }

    // Agora normaliza work → out_path
    // Tokeniza por '/' e resolve "." e ".."
    // Usa uma pilha de componentes em out_path

    // Stack de posições dos componentes (máx 32 níveis)
    int comp_starts[32];
    int comp_ends[32];
    int depth = 0;

    const char *p = work;
    if (*p == '/') p++;  // pula '/' inicial

    while (*p) {
        // Pula barras extras
        while (*p == '/') p++;
        if (*p == '\0') break;

        // Extrai componente
        const char *start = p;
        while (*p && *p != '/') p++;
        int len = (int)(p - start);

        // "." → ignora
        if (len == 1 && start[0] == '.') {
            continue;
        }

        // ".." → volta um nível
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0) depth--;
            continue;
        }

        // Componente normal → push
        if (depth >= 32) return false;  // path muito profundo
        comp_starts[depth] = (int)(start - work);
        comp_ends[depth] = (int)(start - work) + len;
        depth++;
    }

    // Monta o path de saída
    out_path[0] = '/';
    int pos = 1;

    for (int i = 0; i < depth; i++) {
        int clen = comp_ends[i] - comp_starts[i];
        if (pos + clen + 1 >= max) return false;  // overflow

        // Copia componente
        for (int j = 0; j < clen; j++) {
            out_path[pos++] = work[comp_starts[i] + j];
        }

        // Adiciona '/' entre componentes (não no último)
        if (i < depth - 1) {
            out_path[pos++] = '/';
        }
    }

    out_path[pos] = '\0';
    return true;
}

// ============================================================
// vfs_resolve — Resolve path absoluto ou relativo para nó
// ============================================================
vfs_node_t *vfs_resolve(const char *path, vfs_node_t *base_dir,
                         char *resolved_path, int resolved_max) {
    if (!path || !vfs_root) return NULL;

    // Pula espaços
    while (*path == ' ') path++;
    if (*path == '\0') return NULL;

    // Se é absoluto, usa vfs_open diretamente mas com normalização
    // Se é relativo, precisa do base_dir

    // Determina o base_path textual a partir de base_dir
    // Para isso, precisamos ter o path do base_dir. Quem chama deve
    // fornecer resolved_path como buffer, e nós temos current_path externamente.
    // A solução mais limpa: sempre construímos o path canônico primeiro,
    // e depois abrimos via vfs_open.

    // Se path é absoluto
    if (path[0] == '/') {
        // Normaliza
        char canonical[256];
        if (!vfs_build_path("/", path, canonical, 256)) return NULL;

        vfs_node_t *node = vfs_open(canonical);
        if (node && resolved_path) {
            kstrcpy(resolved_path, canonical, resolved_max);
        }
        return node;
    }

    // Path relativo — precisamos do base path textual
    // Importa current_path do shell
    extern char current_path[256];

    // Se temos base_dir, precisamos do path textual correspondente
    // Por ora, usamos current_path (que é o caso 99% do tempo)
    (void)base_dir;

    char canonical[256];
    if (!vfs_build_path(current_path, path, canonical, 256)) return NULL;

    vfs_node_t *node = vfs_open(canonical);
    if (node && resolved_path) {
        kstrcpy(resolved_path, canonical, resolved_max);
    }
    return node;
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
