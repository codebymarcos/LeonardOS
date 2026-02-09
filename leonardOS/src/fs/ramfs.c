// LeonardOS - RamFS (Filesystem em RAM)
// Implementação do backend VFS para arquivos em memória
//
// Estratégia: pool estático de vfs_node_t + ramfs_data_t
// para evitar fragmentação excessiva. Dados de arquivo
// usam kmalloc sob demanda.

#include "ramfs.h"
#include "../memory/heap.h"

// ============================================================
// Pool estático de nós
// ============================================================
static vfs_node_t   node_pool[RAMFS_MAX_NODES];
static ramfs_data_t data_pool[RAMFS_MAX_NODES];
static uint32_t     pool_used = 0;

// ============================================================
// Helpers internos
// ============================================================

static int ramfs_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static void ramfs_strcpy(char *dst, const char *src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void ramfs_memcpy(uint8_t *dst, const uint8_t *src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
}

static void ramfs_memset(uint8_t *dst, uint8_t val, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = val;
}

// ============================================================
// Aloca um nó do pool
// ============================================================
static vfs_node_t *ramfs_alloc_node(void) {
    if (pool_used >= RAMFS_MAX_NODES) return NULL;

    vfs_node_t   *node = &node_pool[pool_used];
    ramfs_data_t *data = &data_pool[pool_used];
    pool_used++;

    // Zera tudo
    ramfs_memset((uint8_t *)node, 0, sizeof(vfs_node_t));
    ramfs_memset((uint8_t *)data, 0, sizeof(ramfs_data_t));

    node->fs_data = data;
    return node;
}

// ============================================================
// Callbacks do VFS — Arquivo
// ============================================================

static uint32_t ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node || !(node->type & VFS_FILE) || !buffer) return 0;

    ramfs_data_t *rd = (ramfs_data_t *)node->fs_data;
    if (!rd || !rd->data) return 0;

    // Verifica limites
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    ramfs_memcpy(buffer, rd->data + offset, size);
    return size;
}

static uint32_t ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    if (!node || !(node->type & VFS_FILE) || !buffer) return 0;

    ramfs_data_t *rd = (ramfs_data_t *)node->fs_data;
    if (!rd) return 0;

    uint32_t end = offset + size;

    // Precisa de mais espaço?
    if (end > rd->capacity) {
        // Limita ao tamanho máximo
        if (end > RAMFS_MAX_FILE_SIZE) {
            end = RAMFS_MAX_FILE_SIZE;
            size = end - offset;
            if (size == 0) return 0;
        }

        // Aloca ou realoca buffer
        uint32_t new_cap = end;
        // Arredonda para múltiplo de 256 para reduzir realocações
        new_cap = (new_cap + 255) & ~255u;
        if (new_cap > RAMFS_MAX_FILE_SIZE) new_cap = RAMFS_MAX_FILE_SIZE;

        uint8_t *new_buf = (uint8_t *)kmalloc(new_cap);
        if (!new_buf) return 0;

        // Copia dados existentes
        if (rd->data && rd->capacity > 0) {
            ramfs_memcpy(new_buf, rd->data, rd->capacity);
            kfree(rd->data);
        }

        // Zera a parte nova
        if (new_cap > rd->capacity) {
            ramfs_memset(new_buf + rd->capacity, 0, new_cap - rd->capacity);
        }

        rd->data = new_buf;
        rd->capacity = new_cap;
    }

    ramfs_memcpy(rd->data + offset, buffer, size);

    // Atualiza tamanho do arquivo
    if (end > node->size) {
        node->size = end;
    }

    return size;
}

// ============================================================
// Callbacks do VFS — Diretório
// ============================================================

static vfs_node_t *ramfs_readdir(vfs_node_t *dir, uint32_t index) {
    if (!dir || !(dir->type & VFS_DIRECTORY)) return NULL;

    ramfs_data_t *rd = (ramfs_data_t *)dir->fs_data;
    if (!rd || index >= rd->child_count) return NULL;

    return rd->children[index];
}

static vfs_node_t *ramfs_finddir(vfs_node_t *dir, const char *name) {
    if (!dir || !(dir->type & VFS_DIRECTORY) || !name) return NULL;

    ramfs_data_t *rd = (ramfs_data_t *)dir->fs_data;
    if (!rd) return NULL;

    for (uint32_t i = 0; i < rd->child_count; i++) {
        if (ramfs_strcmp(rd->children[i]->name, name) == 0) {
            return rd->children[i];
        }
    }

    return NULL;
}

// ============================================================
// Criação de nós
// ============================================================

// Configura um nó como diretório
static void ramfs_setup_dir(vfs_node_t *node, const char *name) {
    ramfs_strcpy(node->name, name, 64);
    node->type = VFS_DIRECTORY;
    node->size = 0;
    node->read = NULL;
    node->write = NULL;
    node->open = NULL;
    node->close = NULL;
    node->readdir = ramfs_readdir;
    node->finddir = ramfs_finddir;
}

// Configura um nó como arquivo
static void ramfs_setup_file(vfs_node_t *node, const char *name) {
    ramfs_strcpy(node->name, name, 64);
    node->type = VFS_FILE;
    node->size = 0;
    node->read = ramfs_read;
    node->write = ramfs_write;
    node->open = NULL;
    node->close = NULL;
    node->readdir = NULL;
    node->finddir = NULL;
}

// Adiciona um filho a um diretório
static bool ramfs_add_child(vfs_node_t *parent, vfs_node_t *child) {
    if (!parent || !(parent->type & VFS_DIRECTORY)) return false;

    ramfs_data_t *rd = (ramfs_data_t *)parent->fs_data;
    if (!rd || rd->child_count >= RAMFS_MAX_CHILDREN) return false;

    ramfs_data_t *cd = (ramfs_data_t *)child->fs_data;
    if (cd) cd->parent = parent;

    rd->children[rd->child_count++] = child;
    return true;
}

// ============================================================
// API pública
// ============================================================

vfs_node_t *ramfs_create_file(vfs_node_t *parent, const char *name) {
    if (!parent || !(parent->type & VFS_DIRECTORY) || !name) return NULL;

    // Verifica se já existe
    if (ramfs_finddir(parent, name) != NULL) {
        // Retorna o existente para overwrite
        return ramfs_finddir(parent, name);
    }

    vfs_node_t *file = ramfs_alloc_node();
    if (!file) return NULL;

    ramfs_setup_file(file, name);

    if (!ramfs_add_child(parent, file)) return NULL;

    return file;
}

vfs_node_t *ramfs_create_dir(vfs_node_t *parent, const char *name) {
    if (!parent || !(parent->type & VFS_DIRECTORY) || !name) return NULL;

    // Verifica se já existe
    vfs_node_t *existing = ramfs_finddir(parent, name);
    if (existing) return existing;

    vfs_node_t *dir = ramfs_alloc_node();
    if (!dir) return NULL;

    ramfs_setup_dir(dir, name);

    if (!ramfs_add_child(parent, dir)) return NULL;

    return dir;
}

// ============================================================
// ramfs_init — Cria o filesystem raiz com estrutura inicial
// ============================================================
vfs_node_t *ramfs_init(void) {
    pool_used = 0;

    // Cria raiz "/"
    vfs_node_t *root = ramfs_alloc_node();
    if (!root) return NULL;
    ramfs_setup_dir(root, "/");

    // Cria diretórios padrão
    ramfs_create_dir(root, "etc");
    ramfs_create_dir(root, "tmp");

    // Cria /etc/hostname com conteúdo padrão
    vfs_node_t *etc = ramfs_finddir(root, "etc");
    if (etc) {
        vfs_node_t *hostname = ramfs_create_file(etc, "hostname");
        if (hostname) {
            const char *name = "leonardos";
            ramfs_write(hostname, 0, 9, (const uint8_t *)name);
        }

        // /etc/version
        vfs_node_t *version = ramfs_create_file(etc, "version");
        if (version) {
            const char *ver = "0.3";
            ramfs_write(version, 0, 3, (const uint8_t *)ver);
        }
    }

    return root;
}
