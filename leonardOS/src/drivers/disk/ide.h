// LeonardOS - Driver IDE/ATA (PIO Mode)
// Acesso a disco ATA via Programmed I/O (polling)
//
// Suporta apenas discos ATA (PATA) no barramento primário.
// Sem DMA, sem interrupções — polling simples.
// API: ide_init, ide_read_sectors, ide_write_sectors

#ifndef __IDE_H__
#define __IDE_H__

#include "../../common/types.h"

// ============================================================
// Constantes ATA
// ============================================================

// Portas do controlador ATA primário
#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERROR        0x1F1
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRIVE_HEAD   0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_COMMAND      0x1F7

// Status bits
#define ATA_SR_BSY   0x80   // Busy
#define ATA_SR_DRDY  0x40   // Drive ready
#define ATA_SR_DRQ   0x08   // Data request
#define ATA_SR_ERR   0x01   // Error

// Comandos ATA
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_FLUSH         0xE7

// Tamanho de setor ATA
#define ATA_SECTOR_SIZE  512

// ============================================================
// Info do disco detectado
// ============================================================
typedef struct {
    bool     present;           // Disco encontrado?
    uint32_t total_sectors;     // Total de setores LBA28
    char     model[41];         // Modelo (string IDENTIFY)
} ide_disk_info_t;

// ============================================================
// API pública
// ============================================================

// Inicializa o driver IDE — detecta disco no barramento primário
// Retorna true se encontrou pelo menos um disco
bool ide_init(void);

// Retorna info do disco detectado
const ide_disk_info_t *ide_get_info(void);

// Lê count setores a partir de lba para buffer
// buffer deve ter pelo menos count * 512 bytes
// Retorna true se sucesso
bool ide_read_sectors(uint32_t lba, uint8_t count, void *buffer);

// Escreve count setores de buffer para lba
// buffer deve ter pelo menos count * 512 bytes
// Retorna true se sucesso
bool ide_write_sectors(uint32_t lba, uint8_t count, const void *buffer);

#endif
