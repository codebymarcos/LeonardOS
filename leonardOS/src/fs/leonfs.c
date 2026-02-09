// LeonardOS - LeonFS (Filesystem persistente em disco)
// Implementação do backend VFS para disco ATA
//
// Estratégia:
//   - Superbloco cached em RAM (flush após mudanças)
//   - Inodes lidos sob demanda do disco
//   - Bitmap de blocos cached parcialmente
//   - Pool de vfs_node_t em RAM (cache de nós abertos)

#include "leonfs.h"
#include "../drivers/disk/ide.h"
#include "../common/string.h"
#include "../common/io.h"

// ============================================================
// Cache em RAM
// ============================================================
static leonfs_superblock_t superblock;
static bool fs_mounted = false;

// Pool de nós VFS para LeonFS (cache de nós abertos)
#define LEONFS_NODE_POOL_SIZE 64
static vfs_node_t   lfs_node_pool[LEONFS_NODE_POOL_SIZE];
static uint32_t     lfs_node_pool_inodes[LEONFS_NODE_POOL_SIZE]; // inode_num de cada nó
static uint32_t     lfs_pool_used = 0;

// Buffer temporário para operações de disco (1 setor)
static uint8_t sector_buf[512];

// ============================================================
// Helpers de disco
// ============================================================

// Lê um setor do disco para o buffer global
static bool read_sector(uint32_t sector) {
    return ide_read_sectors(sector, 1, sector_buf);
}

// Escreve o buffer global para um setor do disco
static bool write_sector(uint32_t sector) {
    return ide_write_sectors(sector, 1, sector_buf);
}

// Lê um setor do disco para um buffer específico
static bool read_sector_to(uint32_t sector, void *buf) {
    return ide_read_sectors(sector, 1, buf);
}

// Escreve de um buffer específico para um setor do disco
static bool write_sector_from(uint32_t sector, const void *buf) {
    return ide_write_sectors(sector, 1, buf);
}

// ============================================================
// Superbloco
// ============================================================

static bool superblock_read(void) {
    if (!read_sector_to(LEONFS_SUPERBLOCK_SECTOR, &superblock)) return false;
    return superblock.magic == LEONFS_MAGIC;
}

static bool superblock_write(void) {
    return write_sector_from(LEONFS_SUPERBLOCK_SECTOR, &superblock);
}

// ============================================================
// Inodes
// ============================================================

// Calcula o setor e offset de um inode
// Cada setor tem 512/64 = 8 inodes
static void inode_location(uint32_t inode_num, uint32_t *sector, uint32_t *offset) {
    *sector = LEONFS_INODE_START + (inode_num / 8);
    *offset = (inode_num % 8) * sizeof(leonfs_inode_t);
}

static bool inode_read(uint32_t inode_num, leonfs_inode_t *inode) {
    if (inode_num >= LEONFS_MAX_INODES) return false;

    uint32_t sec, off;
    inode_location(inode_num, &sec, &off);

    if (!read_sector(sec)) return false;

    kmemcpy(inode, sector_buf + off, sizeof(leonfs_inode_t));
    return true;
}

static bool inode_write(uint32_t inode_num, const leonfs_inode_t *inode) {
    if (inode_num >= LEONFS_MAX_INODES) return false;

    uint32_t sec, off;
    inode_location(inode_num, &sec, &off);

    // Lê setor atual (para não sobrescrever outros inodes)
    if (!read_sector(sec)) return false;

    kmemcpy(sector_buf + off, inode, sizeof(leonfs_inode_t));

    return write_sector(sec);
}

// Aloca um inode livre. Retorna inode_num ou (uint32_t)-1 se falhar.
static uint32_t __attribute__((unused)) inode_alloc(void) {
    leonfs_inode_t inode;
    // Começa em 1 (0 = raiz, já alocada)
    for (uint32_t i = 1; i < LEONFS_MAX_INODES; i++) {
        if (!inode_read(i, &inode)) continue;
        if (inode.type == LEONFS_TYPE_FREE) {
            superblock.free_inodes--;
            superblock_write();
            return i;
        }
    }
    return (uint32_t)-1;
}

// Libera um inode
static void __attribute__((unused)) inode_free(uint32_t inode_num) {
    leonfs_inode_t inode;
    kmemset(&inode, 0, sizeof(leonfs_inode_t));
    inode.type = LEONFS_TYPE_FREE;
    inode_write(inode_num, &inode);
    superblock.free_inodes++;
    superblock_write();
}

// ============================================================
// Bitmap de blocos
// ============================================================

// O bitmap cobre os blocos de dados (a partir de LEONFS_DATA_START)
// Bit N → setor (LEONFS_DATA_START + N)

static bool block_bitmap_get(uint32_t block_num) {
    uint32_t byte_idx = block_num / 8;
    uint32_t bit_idx  = block_num % 8;
    uint32_t bitmap_sector = LEONFS_BITMAP_START + (byte_idx / 512);
    uint32_t sector_offset = byte_idx % 512;

    if (!read_sector(bitmap_sector)) return true; // assume usado se erro

    return (sector_buf[sector_offset] >> bit_idx) & 1;
}

static bool block_bitmap_set(uint32_t block_num, bool used) {
    uint32_t byte_idx = block_num / 8;
    uint32_t bit_idx  = block_num % 8;
    uint32_t bitmap_sector = LEONFS_BITMAP_START + (byte_idx / 512);
    uint32_t sector_offset = byte_idx % 512;

    if (!read_sector(bitmap_sector)) return false;

    if (used) {
        sector_buf[sector_offset] |= (1 << bit_idx);
    } else {
        sector_buf[sector_offset] &= ~(1 << bit_idx);
    }

    return write_sector(bitmap_sector);
}

// Aloca um bloco livre. Retorna número do bloco (offset no data area) ou -1.
static uint32_t block_alloc(void) {
    uint32_t max = superblock.total_blocks;
    if (max > LEONFS_MAX_BLOCKS) max = LEONFS_MAX_BLOCKS;

    for (uint32_t i = 0; i < max; i++) {
        if (!block_bitmap_get(i)) {
            block_bitmap_set(i, true);
            superblock.free_blocks--;
            superblock_write();
            return i;
        }
    }
    return (uint32_t)-1;
}

// Libera um bloco
static void __attribute__((unused)) block_free(uint32_t block_num) {
    block_bitmap_set(block_num, false);
    superblock.free_blocks++;
    superblock_write();
}

// Converte block_num (relativo ao data area) para setor absoluto
static uint32_t block_to_sector(uint32_t block_num) {
    return LEONFS_DATA_START + block_num;
}

// ============================================================
// Pool de nós VFS
// ============================================================

// Busca um nó já aberto pelo inode_num
static vfs_node_t *pool_find(uint32_t inode_num) {
    for (uint32_t i = 0; i < lfs_pool_used; i++) {
        if (lfs_node_pool_inodes[i] == inode_num && lfs_node_pool[i].type != 0) {
            return &lfs_node_pool[i];
        }
    }
    return NULL;
}

// Forward declarations dos callbacks VFS
static uint32_t    leonfs_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t    leonfs_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
static vfs_node_t *leonfs_vfs_readdir(vfs_node_t *dir, uint32_t index);
static vfs_node_t *leonfs_vfs_finddir(vfs_node_t *dir, const char *name);

// Aloca um nó do pool e o inicializa a partir de um inode
static vfs_node_t *pool_alloc(uint32_t inode_num) {
    // Verifica se já existe
    vfs_node_t *existing = pool_find(inode_num);
    if (existing) return existing;

    if (lfs_pool_used >= LEONFS_NODE_POOL_SIZE) return NULL;

    leonfs_inode_t inode;
    if (!inode_read(inode_num, &inode)) return NULL;

    vfs_node_t *node = &lfs_node_pool[lfs_pool_used];
    lfs_node_pool_inodes[lfs_pool_used] = inode_num;
    lfs_pool_used++;

    kmemset(node, 0, sizeof(vfs_node_t));

    // Monta nome — será preenchido pelo chamador (finddir/readdir)
    node->type = (inode.type == LEONFS_TYPE_DIR) ? VFS_DIRECTORY : VFS_FILE;
    node->size = inode.size;

    // Armazena inode_num no fs_data (via cast para uintptr)
    node->fs_data = (void *)(uintptr_t)inode_num;

    // Preenche callbacks
    if (node->type & VFS_FILE) {
        node->read  = leonfs_vfs_read;
        node->write = leonfs_vfs_write;
    }
    if (node->type & VFS_DIRECTORY) {
        node->readdir = leonfs_vfs_readdir;
        node->finddir = leonfs_vfs_finddir;
    }

    return node;
}

// Recupera inode_num de um vfs_node
static uint32_t node_inode_num(vfs_node_t *node) {
    return (uint32_t)(uintptr_t)node->fs_data;
}

// ============================================================
// Callbacks VFS — Arquivo
// ============================================================

static uint32_t leonfs_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node || !buffer || !(node->type & VFS_FILE)) return 0;

    uint32_t inum = node_inode_num(node);
    leonfs_inode_t inode;
    if (!inode_read(inum, &inode)) return 0;

    if (offset >= inode.size) return 0;
    if (offset + size > inode.size) size = inode.size - offset;

    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        // Qual bloco contém o offset atual?
        uint32_t file_offset = offset + bytes_read;
        uint32_t block_idx = file_offset / LEONFS_BLOCK_SIZE;
        uint32_t block_off = file_offset % LEONFS_BLOCK_SIZE;

        if (block_idx >= LEONFS_DIRECT_BLOCKS) break;
        if (inode.blocks[block_idx] == 0) break;

        uint32_t sector = block_to_sector(inode.blocks[block_idx]);
        if (!read_sector_to(sector, sector_buf)) break;

        // Quantos bytes podemos ler deste bloco?
        uint32_t chunk = LEONFS_BLOCK_SIZE - block_off;
        if (chunk > size - bytes_read) chunk = size - bytes_read;

        kmemcpy(buffer + bytes_read, sector_buf + block_off, chunk);
        bytes_read += chunk;
    }

    return bytes_read;
}

static uint32_t leonfs_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    if (!node || !buffer || !(node->type & VFS_FILE)) return 0;

    uint32_t inum = node_inode_num(node);
    leonfs_inode_t inode;
    if (!inode_read(inum, &inode)) return 0;

    if (offset + size > LEONFS_MAX_FILE_SIZE) {
        size = LEONFS_MAX_FILE_SIZE - offset;
        if (size == 0) return 0;
    }

    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t file_offset = offset + bytes_written;
        uint32_t block_idx = file_offset / LEONFS_BLOCK_SIZE;
        uint32_t block_off = file_offset % LEONFS_BLOCK_SIZE;

        if (block_idx >= LEONFS_DIRECT_BLOCKS) break;

        // Aloca bloco se necessário
        if (inode.blocks[block_idx] == 0) {
            uint32_t new_block = block_alloc();
            if (new_block == (uint32_t)-1) break;
            inode.blocks[block_idx] = new_block;

            // Zera o bloco novo
            kmemset(sector_buf, 0, LEONFS_BLOCK_SIZE);
            write_sector_from(block_to_sector(new_block), sector_buf);
        }

        uint32_t sector = block_to_sector(inode.blocks[block_idx]);

        // Lê bloco atual (para não perder dados em escrita parcial)
        if (!read_sector_to(sector, sector_buf)) break;

        // Quantos bytes podemos escrever neste bloco?
        uint32_t chunk = LEONFS_BLOCK_SIZE - block_off;
        if (chunk > size - bytes_written) chunk = size - bytes_written;

        kmemcpy(sector_buf + block_off, buffer + bytes_written, chunk);

        if (!write_sector_from(sector, sector_buf)) break;

        bytes_written += chunk;
    }

    // Atualiza tamanho
    uint32_t new_end = offset + bytes_written;
    if (new_end > inode.size) {
        inode.size = new_end;
    }

    // Persiste inode
    inode_write(inum, &inode);

    // Atualiza cache VFS
    node->size = inode.size;

    return bytes_written;
}

// ============================================================
// Callbacks VFS — Diretório
// ============================================================

static vfs_node_t *leonfs_vfs_readdir(vfs_node_t *dir, uint32_t index) {
    if (!dir || !(dir->type & VFS_DIRECTORY)) return NULL;

    uint32_t inum = node_inode_num(dir);
    leonfs_inode_t inode;
    if (!inode_read(inum, &inode)) return NULL;

    // Itera pelas entradas de diretório
    uint32_t entry_idx = 0;
    for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS; b++) {
        if (inode.blocks[b] == 0) continue;

        uint32_t sector = block_to_sector(inode.blocks[b]);
        if (!read_sector_to(sector, sector_buf)) continue;

        leonfs_dir_entry_t *entries = (leonfs_dir_entry_t *)sector_buf;
        for (uint32_t e = 0; e < LEONFS_DIR_ENTRIES_PER_BLOCK; e++) {
            if (entries[e].inode_num == 0 && entries[e].name[0] == '\0') continue;
            // Pula entradas vazias (inode_num pode ser 0 para raiz, mas name vazio = vazio)
            if (entries[e].name[0] == '\0') continue;

            if (entry_idx == index) {
                // Salva dados ANTES de pool_alloc (que sobrescreve sector_buf)
                uint32_t child_inum = entries[e].inode_num;
                char child_name[64];
                kstrcpy(child_name, entries[e].name, 64);

                vfs_node_t *child = pool_alloc(child_inum);
                if (child) {
                    kstrcpy(child->name, child_name, 64);
                }
                return child;
            }
            entry_idx++;
        }
    }

    return NULL;
}

static vfs_node_t *leonfs_vfs_finddir(vfs_node_t *dir, const char *name) {
    if (!dir || !(dir->type & VFS_DIRECTORY) || !name) return NULL;

    uint32_t inum = node_inode_num(dir);
    leonfs_inode_t inode;
    if (!inode_read(inum, &inode)) return NULL;

    for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS; b++) {
        if (inode.blocks[b] == 0) continue;

        uint32_t sector = block_to_sector(inode.blocks[b]);
        if (!read_sector_to(sector, sector_buf)) continue;

        leonfs_dir_entry_t *entries = (leonfs_dir_entry_t *)sector_buf;
        for (uint32_t e = 0; e < LEONFS_DIR_ENTRIES_PER_BLOCK; e++) {
            if (entries[e].name[0] == '\0') continue;

            if (kstrcmp(entries[e].name, name) == 0) {
                // Salva dados ANTES de pool_alloc (que sobrescreve sector_buf)
                uint32_t child_inum = entries[e].inode_num;
                char child_name[64];
                kstrcpy(child_name, entries[e].name, 64);

                vfs_node_t *child = pool_alloc(child_inum);
                if (child) {
                    kstrcpy(child->name, child_name, 64);
                }
                return child;
            }
        }
    }

    return NULL;
}

// ============================================================
// Criação de arquivos e diretórios
// ============================================================

// Adiciona uma entrada a um diretório no disco
static bool __attribute__((unused)) dir_add_entry(uint32_t dir_inum, uint32_t child_inum, const char *name) {
    leonfs_inode_t dir_inode;
    if (!inode_read(dir_inum, &dir_inode)) return false;

    // Procura slot vazio nos blocos existentes
    for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS; b++) {
        if (dir_inode.blocks[b] == 0) continue;

        uint32_t sector = block_to_sector(dir_inode.blocks[b]);
        if (!read_sector_to(sector, sector_buf)) continue;

        leonfs_dir_entry_t *entries = (leonfs_dir_entry_t *)sector_buf;
        for (uint32_t e = 0; e < LEONFS_DIR_ENTRIES_PER_BLOCK; e++) {
            if (entries[e].name[0] == '\0') {
                // Slot vazio — usa
                entries[e].inode_num = child_inum;
                kstrcpy(entries[e].name, name, LEONFS_MAX_NAME);
                return write_sector_from(sector, sector_buf);
            }
        }
    }

    // Todos os blocos cheios — aloca novo bloco pro diretório
    for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS; b++) {
        if (dir_inode.blocks[b] == 0) {
            uint32_t new_block = block_alloc();
            if (new_block == (uint32_t)-1) return false;

            dir_inode.blocks[b] = new_block;

            // Zera e escreve a primeira entrada
            kmemset(sector_buf, 0, LEONFS_BLOCK_SIZE);
            leonfs_dir_entry_t *entries = (leonfs_dir_entry_t *)sector_buf;
            entries[0].inode_num = child_inum;
            kstrcpy(entries[0].name, name, LEONFS_MAX_NAME);

            write_sector_from(block_to_sector(new_block), sector_buf);

            // Atualiza tamanho do diretório
            dir_inode.size += LEONFS_BLOCK_SIZE;
            inode_write(dir_inum, &dir_inode);
            return true;
        }
    }

    return false; // Diretório cheio (10 blocos = 80 entradas)
}

// Remove uma entrada de um diretório
static bool __attribute__((unused)) dir_remove_entry(uint32_t dir_inum, const char *name) {
    leonfs_inode_t dir_inode;
    if (!inode_read(dir_inum, &dir_inode)) return false;

    for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS; b++) {
        if (dir_inode.blocks[b] == 0) continue;

        uint32_t sector = block_to_sector(dir_inode.blocks[b]);
        if (!read_sector_to(sector, sector_buf)) continue;

        leonfs_dir_entry_t *entries = (leonfs_dir_entry_t *)sector_buf;
        for (uint32_t e = 0; e < LEONFS_DIR_ENTRIES_PER_BLOCK; e++) {
            if (entries[e].name[0] != '\0' && kstrcmp(entries[e].name, name) == 0) {
                // Zera a entrada
                kmemset(&entries[e], 0, sizeof(leonfs_dir_entry_t));
                return write_sector_from(sector, sector_buf);
            }
        }
    }
    return false;
}

// ============================================================
// leonfs_format — Formata o disco com LeonFS
// ============================================================
bool leonfs_format(void) {
    const ide_disk_info_t *disk = ide_get_info();
    if (!disk || !disk->present) return false;

    // Calcula blocos de dados disponíveis
    uint32_t data_sectors = disk->total_sectors - LEONFS_DATA_START;
    if (data_sectors > LEONFS_MAX_BLOCKS) data_sectors = LEONFS_MAX_BLOCKS;

    // --- Escreve superbloco ---
    kmemset(&superblock, 0, sizeof(superblock));
    superblock.magic        = LEONFS_MAGIC;
    superblock.version      = 1;
    superblock.total_blocks = data_sectors;
    superblock.free_blocks  = data_sectors;
    superblock.total_inodes = LEONFS_MAX_INODES;
    superblock.free_inodes  = LEONFS_MAX_INODES - 1; // inode 0 = raiz
    superblock.root_inode   = 0;
    superblock_write();

    // --- Zera bitmap de blocos ---
    kmemset(sector_buf, 0, 512);
    for (uint32_t s = 0; s < LEONFS_BITMAP_SECTORS; s++) {
        write_sector_from(LEONFS_BITMAP_START + s, sector_buf);
    }

    // --- Zera tabela de inodes ---
    for (uint32_t s = 0; s < LEONFS_INODE_SECTORS; s++) {
        write_sector_from(LEONFS_INODE_START + s, sector_buf);
    }

    // --- Cria inode raiz (inode 0 = diretório raiz) ---
    leonfs_inode_t root_inode;
    kmemset(&root_inode, 0, sizeof(root_inode));
    root_inode.type = LEONFS_TYPE_DIR;
    root_inode.size = 0;

    // Aloca um bloco para o conteúdo do diretório raiz
    // (Marca bloco 0 como usado no bitmap)
    block_bitmap_set(0, true);
    superblock.free_blocks--;
    root_inode.blocks[0] = 0; // bloco 0 do data area
    root_inode.size = LEONFS_BLOCK_SIZE;

    // Zera o bloco de dados da raiz
    kmemset(sector_buf, 0, 512);
    write_sector_from(block_to_sector(0), sector_buf);

    // Escreve inode raiz
    inode_write(0, &root_inode);

    // Atualiza superbloco final
    superblock_write();

    return true;
}

// ============================================================
// leonfs_init — Inicializa LeonFS
// ============================================================
vfs_node_t *leonfs_init(void) {
    lfs_pool_used = 0;
    kmemset(lfs_node_pool, 0, sizeof(lfs_node_pool));
    kmemset(lfs_node_pool_inodes, 0, sizeof(lfs_node_pool_inodes));

    // Tenta ler superbloco
    if (!superblock_read()) {
        // Disco não tem LeonFS — formata
        if (!leonfs_format()) return NULL;
        // Relê superbloco
        if (!superblock_read()) return NULL;
    }

    fs_mounted = true;

    // Cria nó VFS para raiz
    vfs_node_t *root = pool_alloc(superblock.root_inode);
    if (root) {
        kstrcpy(root->name, "mnt", 64);
    }

    return root;
}

// ============================================================
// leonfs_create_file — Cria um arquivo em um diretório
// ============================================================
vfs_node_t *leonfs_create_file(vfs_node_t *parent, const char *name) {
    if (!parent || !(parent->type & VFS_DIRECTORY) || !name) return NULL;

    uint32_t parent_inum = node_inode_num(parent);

    // Verifica se já existe
    vfs_node_t *existing = leonfs_vfs_finddir(parent, name);
    if (existing) return existing;

    // Aloca inode novo
    uint32_t new_inum = inode_alloc();
    if (new_inum == (uint32_t)-1) return NULL;

    // Cria inode arquivo
    leonfs_inode_t inode;
    kmemset(&inode, 0, sizeof(leonfs_inode_t));
    inode.type = LEONFS_TYPE_FILE;
    inode.size = 0;

    if (!inode_write(new_inum, &inode)) {
        inode_free(new_inum);
        return NULL;
    }

    // Adiciona entrada ao diretório pai
    if (!dir_add_entry(parent_inum, new_inum, name)) {
        inode_free(new_inum);
        return NULL;
    }

    // Cria nó VFS e retorna
    vfs_node_t *file = pool_alloc(new_inum);
    if (file) {
        kstrcpy(file->name, name, 64);
    }

    return file;
}

// ============================================================
// leonfs_create_dir — Cria um diretório em um diretório
// ============================================================
vfs_node_t *leonfs_create_dir(vfs_node_t *parent, const char *name) {
    if (!parent || !(parent->type & VFS_DIRECTORY) || !name) return NULL;

    uint32_t parent_inum = node_inode_num(parent);

    // Verifica se já existe
    vfs_node_t *existing = leonfs_vfs_finddir(parent, name);
    if (existing) return existing;

    // Aloca inode novo
    uint32_t new_inum = inode_alloc();
    if (new_inum == (uint32_t)-1) return NULL;

    // Aloca bloco para o conteúdo do diretório
    uint32_t dir_block = block_alloc();
    if (dir_block == (uint32_t)-1) {
        inode_free(new_inum);
        return NULL;
    }

    // Zera o bloco do diretório
    kmemset(sector_buf, 0, LEONFS_BLOCK_SIZE);
    write_sector_from(block_to_sector(dir_block), sector_buf);

    // Cria inode diretório
    leonfs_inode_t inode;
    kmemset(&inode, 0, sizeof(leonfs_inode_t));
    inode.type = LEONFS_TYPE_DIR;
    inode.size = LEONFS_BLOCK_SIZE;
    inode.blocks[0] = dir_block;

    if (!inode_write(new_inum, &inode)) {
        inode_free(new_inum);
        block_free(dir_block);
        return NULL;
    }

    // Adiciona entrada ao diretório pai
    if (!dir_add_entry(parent_inum, new_inum, name)) {
        inode_free(new_inum);
        block_free(dir_block);
        return NULL;
    }

    // Cria nó VFS e retorna
    vfs_node_t *dir = pool_alloc(new_inum);
    if (dir) {
        kstrcpy(dir->name, name, 64);
    }

    return dir;
}

// ============================================================
// leonfs_is_node — Verifica se um nó pertence ao LeonFS
// ============================================================
bool leonfs_is_node(vfs_node_t *node) {
    if (!node || !fs_mounted) return false;
    // Checa se o ponteiro está dentro do pool de nós LeonFS
    return (node >= &lfs_node_pool[0] && node < &lfs_node_pool[LEONFS_NODE_POOL_SIZE]);
}

// ============================================================
// leonfs_remove — Remove arquivo ou diretório vazio
// ============================================================
bool leonfs_remove(vfs_node_t *parent, const char *name) {
    if (!parent || !(parent->type & VFS_DIRECTORY) || !name) return false;

    uint32_t parent_inum = node_inode_num(parent);

    // Encontra o inode do alvo via dir entries
    leonfs_inode_t parent_inode;
    if (!inode_read(parent_inum, &parent_inode)) return false;

    uint32_t target_inum = (uint32_t)-1;
    for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS && target_inum == (uint32_t)-1; b++) {
        if (parent_inode.blocks[b] == 0) continue;
        uint32_t sector = block_to_sector(parent_inode.blocks[b]);
        if (!read_sector_to(sector, sector_buf)) continue;
        leonfs_dir_entry_t *entries = (leonfs_dir_entry_t *)sector_buf;
        for (uint32_t e = 0; e < LEONFS_DIR_ENTRIES_PER_BLOCK; e++) {
            if (entries[e].name[0] != '\0' && kstrcmp(entries[e].name, name) == 0) {
                target_inum = entries[e].inode_num;
                break;
            }
        }
    }

    if (target_inum == (uint32_t)-1) return false;

    // Lê o inode do alvo
    leonfs_inode_t target_inode;
    if (!inode_read(target_inum, &target_inode)) return false;

    // Se é diretório, verifica se está vazio
    if (target_inode.type == LEONFS_TYPE_DIR) {
        for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS; b++) {
            if (target_inode.blocks[b] == 0) continue;
            uint32_t sector = block_to_sector(target_inode.blocks[b]);
            if (!read_sector_to(sector, sector_buf)) continue;
            leonfs_dir_entry_t *entries = (leonfs_dir_entry_t *)sector_buf;
            for (uint32_t e = 0; e < LEONFS_DIR_ENTRIES_PER_BLOCK; e++) {
                if (entries[e].name[0] != '\0') return false; // não vazio
            }
        }
    }

    // Libera blocos de dados do alvo
    for (uint32_t b = 0; b < LEONFS_DIRECT_BLOCKS; b++) {
        if (target_inode.blocks[b] != 0) {
            block_free(target_inode.blocks[b]);
        }
    }

    // Libera inode
    inode_free(target_inum);

    // Remove entrada do diretório pai
    dir_remove_entry(parent_inum, name);

    // Invalida o nó no pool VFS (se existir)
    for (uint32_t i = 0; i < lfs_pool_used; i++) {
        if (lfs_node_pool_inodes[i] == target_inum && lfs_node_pool[i].type != 0) {
            lfs_node_pool[i].type = 0;
            lfs_node_pool[i].name[0] = '\0';
            break;
        }
    }

    return true;
}

// ============================================================
// leonfs_get_superblock — Expõe o superbloco em cache
// ============================================================
leonfs_superblock_t *leonfs_get_superblock(void) {
    if (!fs_mounted) return NULL;
    return &superblock;
}
