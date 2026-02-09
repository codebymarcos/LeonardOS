// LeonardOS - Driver IDE/ATA (PIO Mode)
// Implementação de acesso a disco via polling
//
// Usa LBA28 (endereçamento de 28 bits → suporta até ~128GB)
// Apenas barramento primário, drive master.

#include "ide.h"
#include "../../common/io.h"
#include "../../common/string.h"

// ============================================================
// Estado global
// ============================================================
static ide_disk_info_t disk_info;

// ============================================================
// Helpers internos
// ============================================================

// Espera o disco ficar pronto (BSY=0)
static bool ide_wait_ready(void) {
    // Timeout: ~1 milhão de iterações (~5 segundos em hardware real)
    for (int i = 0; i < 1000000; i++) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY)) return true;
    }
    return false;  // timeout
}

// Espera DRQ (dados prontos para transferência)
static bool ide_wait_drq(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_ERR) return false;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return true;
    }
    return false;  // timeout
}

// Delay de 400ns lendo status 4 vezes (spec ATA)
static void ide_400ns_delay(void) {
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
}

// ============================================================
// ide_init — Detecta disco ATA no barramento primário
// ============================================================
bool ide_init(void) {
    kmemset(&disk_info, 0, sizeof(disk_info));

    // Seleciona drive master
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0);
    ide_400ns_delay();

    // Zera portas de setor
    outb(ATA_PRIMARY_SECTOR_COUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);

    // Envia IDENTIFY
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    ide_400ns_delay();

    // Lê status
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        // Nenhum disco presente
        return false;
    }

    // Espera BSY limpar
    if (!ide_wait_ready()) return false;

    // Verifica se é ATA (não ATAPI): LBA_MID e LBA_HI devem ser 0
    uint8_t mid = inb(ATA_PRIMARY_LBA_MID);
    uint8_t hi  = inb(ATA_PRIMARY_LBA_HI);
    if (mid != 0 || hi != 0) {
        // Não é ATA (pode ser ATAPI/CD-ROM)
        return false;
    }

    // Espera DRQ
    if (!ide_wait_drq()) return false;

    // Lê 256 words (512 bytes) do buffer IDENTIFY
    uint16_t identify[256];
    insw(ATA_PRIMARY_DATA, identify, 256);

    // Extrai total de setores LBA28 (words 60-61)
    disk_info.total_sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);

    // Extrai modelo (words 27-46, 20 words = 40 chars, byte-swapped)
    for (int i = 0; i < 20; i++) {
        disk_info.model[i * 2]     = (char)(identify[27 + i] >> 8);
        disk_info.model[i * 2 + 1] = (char)(identify[27 + i] & 0xFF);
    }
    disk_info.model[40] = '\0';

    // Trim trailing spaces do modelo
    for (int i = 39; i >= 0; i--) {
        if (disk_info.model[i] == ' ' || disk_info.model[i] == '\0') {
            disk_info.model[i] = '\0';
        } else {
            break;
        }
    }

    disk_info.present = true;
    return true;
}

// ============================================================
// ide_get_info — Retorna info do disco
// ============================================================
const ide_disk_info_t *ide_get_info(void) {
    return &disk_info;
}

// ============================================================
// ide_read_sectors — Lê setores do disco (PIO, LBA28)
// ============================================================
bool ide_read_sectors(uint32_t lba, uint8_t count, void *buffer) {
    if (!disk_info.present || !buffer || count == 0) return false;
    if (lba + count > disk_info.total_sectors) return false;

    // Espera disco pronto
    if (!ide_wait_ready()) return false;

    // Seleciona drive master + bits altos do LBA
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));

    // Configura parâmetros
    outb(ATA_PRIMARY_SECTOR_COUNT, count);
    outb(ATA_PRIMARY_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));

    // Envia comando READ SECTORS
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);

    // Lê cada setor
    uint16_t *buf16 = (uint16_t *)buffer;
    for (int s = 0; s < count; s++) {
        // Espera DRQ
        if (!ide_wait_drq()) return false;

        // Lê 256 words (512 bytes)
        insw(ATA_PRIMARY_DATA, buf16, 256);
        buf16 += 256;
    }

    return true;
}

// ============================================================
// ide_write_sectors — Escreve setores no disco (PIO, LBA28)
// ============================================================
bool ide_write_sectors(uint32_t lba, uint8_t count, const void *buffer) {
    if (!disk_info.present || !buffer || count == 0) return false;
    if (lba + count > disk_info.total_sectors) return false;

    // Espera disco pronto
    if (!ide_wait_ready()) return false;

    // Seleciona drive master + bits altos do LBA
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));

    // Configura parâmetros
    outb(ATA_PRIMARY_SECTOR_COUNT, count);
    outb(ATA_PRIMARY_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));

    // Envia comando WRITE SECTORS
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);

    // Escreve cada setor
    const uint16_t *buf16 = (const uint16_t *)buffer;
    for (int s = 0; s < count; s++) {
        // Espera DRQ
        if (!ide_wait_drq()) return false;

        // Escreve 256 words (512 bytes)
        outsw(ATA_PRIMARY_DATA, buf16, 256);
        buf16 += 256;
    }

    // Flush cache
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_FLUSH);
    if (!ide_wait_ready()) return false;

    return true;
}
