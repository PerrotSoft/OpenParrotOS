#include <stdint.h>
#include "../io.h"

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
#define IDE_BASE        0x1F0   // Data/Command блок
#define IDE_CTRL        0x3F6   // Alternate status / Device control

// Регистры (смещения от IDE_BASE)
#define ATA_REG_DATA        (IDE_BASE + 0)
#define ATA_REG_ERROR       (IDE_BASE + 1)
#define ATA_REG_FEATURES    (IDE_BASE + 1)
#define ATA_REG_SECCNT      (IDE_BASE + 2) // sector count
#define ATA_REG_LBA0        (IDE_BASE + 3) // LBA[7:0]
#define ATA_REG_LBA1        (IDE_BASE + 4) // LBA[15:8]
#define ATA_REG_LBA2        (IDE_BASE + 5) // LBA[23:16]
#define ATA_REG_HDDEVSEL    (IDE_BASE + 6) // drive/head (LBA[27:24])
#define ATA_REG_COMMAND     (IDE_BASE + 7)
#define ATA_REG_STATUS      (IDE_BASE + 7)

// Биты статуса
#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_DRDY 0x40
#define ATA_SR_BSY  0x80

// Команды
#define ATA_CMD_READ_SECTORS     0x20
#define ATA_CMD_WRITE_SECTORS    0x30
#define ATA_CMD_IDENTIFY         0xEC

// Управление (ALT/CTRL порт 0x3F6)
#define ATA_DC_nIEN  0x02  // 1 = запрет IRQ (мы используем PIO без IRQ)
#define ATA_DC_SRST  0x04  // soft reset

// Лимиты ожиданий (подбираются опытно; для QEMU почти мгновенно)
#define ATA_POLL_LIMIT_READY  1000000
#define ATA_POLL_LIMIT_DRQ    1000000

// 400 нс ожидание — 4 чтения ALT-статуса
static inline void ata_400ns(void) {
    (void)inb(IDE_CTRL);
    (void)inb(IDE_CTRL);
    (void)inb(IDE_CTRL);
    (void)inb(IDE_CTRL);
}

// Общая проверка ошибок статуса
static inline int ata_status_error(uint8_t st) {
    if (st & ATA_SR_ERR) return -2; // устройство сообщило ошибку
    if (st & ATA_SR_DF)  return -3; // device fault
    return 0;
}

// Ждать пока BSY=0 (готовность устройства)
static int ide_wait_ready(void) {
    for (int i = 0; i < ATA_POLL_LIMIT_READY; i++) {
        uint8_t st = inb(ATA_REG_STATUS);
        if (!(st & ATA_SR_BSY)) {
            // Дополнительно убедимся, что устройство готово к командам (DRDY может отсутствовать на некоторых)
            int err = ata_status_error(st);
            if (err) return err;
            return 0;
        }
    }
    return -1; // timeout
}

// Ждать пока DRQ=1 (данные готовы/принимаются)
static int ide_wait_drq(void) {
    for (int i = 0; i < ATA_POLL_LIMIT_DRQ; i++) {
        uint8_t st = inb(ATA_REG_STATUS);
        if (st & ATA_SR_DRQ) {
            int err = ata_status_error(st);
            if (err) return err;
            return 0;
        }
        if (!(st & ATA_SR_BSY)) {
            // Если не занято и нет DRQ — проверим ошибки
            int err = ata_status_error(st);
            if (err) return err;
        }
    }
    return -1; // timeout
}

uint8_t current_drive = 0; // 0 = master, 1 = slave

void ide_select_drive(uint8_t drive) {
    current_drive = drive;
}

static inline void ide_select_lba28(uint32_t lba) {
    uint8_t devsel = (current_drive == 0) ? 0xE0 : 0xF0;
    outb(ATA_REG_HDDEVSEL, devsel | ((lba >> 24) & 0x0F));
    ata_400ns();
}


// Инициализирующая последовательность перед командами R/W
static int ide_setup_rw(uint32_t lba, uint8_t count) {
    int st = ide_wait_ready();
    if (st < 0) return st;

    // Отключим IRQ для PIO через ALT-порт (не обязательно, но аккуратно)
    outb(IDE_CTRL, ATA_DC_nIEN);

    ide_select_lba28(lba);

    outb(ATA_REG_SECCNT, count ? count : 1);     // 0 -> трактуется как 256, здесь читаем/пишем ровно 1
    outb(ATA_REG_LBA0,   (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1,   (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2,   (uint8_t)((lba >> 16) & 0xFF));

    return 0;
}

// --- Чтение сектора (512 байт) ---
int ide_read_sector(uint32_t lba, uint16_t* buffer) {
    int rc = ide_setup_rw(lba, 1);
    if (rc < 0) return rc;

    outb(ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    ata_400ns(); // минимальная пауза после команды

    rc = ide_wait_drq();
    if (rc < 0) return rc;

    // Читаем ровно 256 слов по 16 бит
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(ATA_REG_DATA);
    }

    // Завершающее ожидание (BSY=0) — некоторые устройства требуют
    rc = ide_wait_ready();
    if (rc < 0) return rc;

    return 0;
}

// --- Запись сектора (512 байт) ---
int ide_write_sector(uint32_t lba, const uint16_t* buffer) {
    int rc = ide_setup_rw(lba, 1);
    if (rc < 0) return rc;

    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    ata_400ns();

    rc = ide_wait_drq();
    if (rc < 0) return rc;

    for (int i = 0; i < 256; i++) {
        outw(ATA_REG_DATA, buffer[i]);
    }

    // Некоторые контроллеры требуют «flush» ожидания окончания внутренней записи
    rc = ide_wait_ready();
    if (rc < 0) return rc;

    return 0;
}

// --- IDENTIFY DEVICE ---
int ide_identify(uint16_t* buffer) {
    int rc = ide_wait_ready();
    if (rc < 0) return rc;

    // master + LBA bit без значения lba, но порядок такой же
    outb(IDE_CTRL, ATA_DC_nIEN);
    outb(ATA_REG_HDDEVSEL, 0xA0); // master, CHS/LBA неважно для IDENTIFY
    ata_400ns();

    outb(ATA_REG_SECCNT, 0);
    outb(ATA_REG_LBA0,   0);
    outb(ATA_REG_LBA1,   0);
    outb(ATA_REG_LBA2,   0);

    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_400ns();

    // Если статус = 0 — устройства нет
    uint8_t st = inb(ATA_REG_STATUS);
    if (st == 0) return -4;

    rc = ide_wait_drq();
    if (rc < 0) return rc;

    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(ATA_REG_DATA);
    }

    return 0;
}

// --- Получить размер диска в секторах ---
uint32_t ide_get_sector_count(void) {
    uint16_t id_sector[256];
    if (ide_identify(id_sector) < 0) return 0;

    // Слова 60–61 = количество LBA28 секторов (младшее слово, затем старшее)
    return ((uint32_t)id_sector[61] << 16) | id_sector[60];
}

// --- Чтение/запись байта (read-modify-write сектора) ---
uint8_t ide_read_byte(uint32_t index) {
    uint16_t buf[256];
    uint32_t sector = index / 512;
    uint32_t offset = index % 512;

    if (ide_read_sector(sector, buf) < 0) return 0xFF;
    return ((const uint8_t*)buf)[offset];
}

void ide_write_byte(uint32_t index, uint8_t value) {
    uint16_t buf[256];
    uint32_t sector = index / 512;
    uint32_t offset = index % 512;

    if (ide_read_sector(sector, buf) < 0) return;
    ((uint8_t*)buf)[offset] = value;
    (void)ide_write_sector(sector, buf);
}
