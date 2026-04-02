/* audio_driver.c - реализация (скелет) */

#include "../drivers/audio_driver.h"
#include <stddef.h>  // для uintptr_t
uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__ ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uintptr_t virt_to_phys(void* addr) {
    return (uintptr_t)addr; // если нет MMU
}
/* ====== AC'97/NABM регистры (упрощённо, см. предыдущий пример) ====== */
enum {
    AC97_RESET        = 0x00,
    AC97_MASTER_VOL   = 0x02,
    AC97_PCM_OUT_VOL  = 0x18,
    AC97_PCM_FRONT_DAC_RATE = 0x2C,
};

enum {
    PO_BDBAR = 0x10,
    PO_CIV   = 0x14, /* 8-bit */
    PO_LVI   = 0x15, /* 8-bit */
    PO_SR    = 0x16, /* status (w1c) */
    PO_CR    = 0x1B, /* control */
};

#define PO_CR_RUN   0x01
#define PO_CR_IOCE  0x04

/* PO_SR bits (примерные) */
#define PO_SR_LVBCI 0x04
#define PO_SR_BCIS  0x08
#define PO_SR_DCH   0x01

/* Внутрен хелперы для чтения/записи NAM/NABM */
static inline void nam_writew(audio_driver_t* d, uint16_t reg, uint16_t val) {
    outw(d->nam_base + reg, val);
}
static inline uint16_t nam_readw(audio_driver_t* d, uint16_t reg) {
    return inw(d->nam_base + reg);
}
static inline void nabm_writel(audio_driver_t* d, uint16_t off, uint32_t val) {
    outl(d->nabm_base + off, val);
}
static inline void nabm_writeb(audio_driver_t* d, uint16_t off, uint8_t val) {
    outb(d->nabm_base + off, val);
}
static inline uint8_t nabm_readb(audio_driver_t* d, uint16_t off) {
    return inb(d->nabm_base + off);
}

/* Инициализация структуры драйвера */
int audio_driver_init(audio_driver_t* drv,
                      uint16_t nam_base, uint16_t nabm_base,
                      uint32_t bdl_phys_address)
{
    if (!drv) return -1;
    drv->nam_base = nam_base;
    drv->nabm_base = nabm_base;
    drv->bdl_phys = bdl_phys_address;
    drv->entries = 0;
    drv->running = 0;
    drv->paused = 0;
    drv->on_buffer_done = NULL;
    drv->sample_rate = 44100;
    drv->channels = 2;
    drv->bits_per_sample = 16;

    /* Reset codec */
    nam_writew(drv, AC97_RESET, 0);
    /* Можно тут delay; в ядре вставь соответствующую паузу */

    /* Установим громкость по умолчанию (0x0000 = max) */
    nam_writew(drv, AC97_MASTER_VOL, 0x0000);
    nam_writew(drv, AC97_PCM_OUT_VOL, 0x0000);

    /* Установим частоту, если контроллер поддерживает прямую запись регистра */
    nam_writew(drv, AC97_PCM_FRONT_DAC_RATE, (uint16_t)drv->sample_rate);

    /* Очистим статус PCM Out */
    nabm_writeb(drv, PO_CR, 0);
    nabm_writeb(drv, PO_SR, PO_SR_LVBCI | PO_SR_BCIS | PO_SR_DCH);

    /* Запишем phys адрес BDL в регистр */
    nabm_writel(drv, PO_BDBAR, drv->bdl_phys);

    return 0;
}

/* Установить формат потока (только метаданные, проверка в play) */
int audio_driver_set_format(audio_driver_t* drv,
                            uint32_t sample_rate,
                            uint16_t channels,
                            uint16_t bits_per_sample)
{
    if (!drv) return -1;
    drv->sample_rate = sample_rate;
    drv->channels = channels;
    drv->bits_per_sample = bits_per_sample;

    /* Попробуем записать скорость в регистр (если поддерживается) */
    nam_writew(drv, AC97_PCM_FRONT_DAC_RATE, (uint16_t)sample_rate);
    return 0;
}

/* Подготовить BDL: передаём массив указателей на PCM и их длины
   Драйвер скопирует физические адреса в BDL.
   count <= AUDIO_MAX_BDL
*/
int audio_driver_prepare_play(audio_driver_t* drv,
                              const void* pcm_ptrs[], const uint32_t lengths[], uint16_t count)
{
    if (!drv || !pcm_ptrs || !lengths || count == 0 || count > AUDIO_MAX_BDL) return -1;

    /* Проверка формата: драйвер ожидает 16-bit PCM в примере (можно расширить) */
    if (drv->bits_per_sample != 16 && drv->bits_per_sample != 8) {
        /* можно поддержать, но пока - отказ */
        return -2;
    }

    /* Заполняем BDL и массивы */
    for (uint16_t i = 0; i < count; ++i) {
        const void* vptr = pcm_ptrs[i];
        uint32_t len = lengths[i];
        if (!vptr || len == 0) return -3;

        uint32_t phys = virt_to_phys(vptr);
        drv->bdl[i].addr_phys = phys;
        /* в AC'97 обычное ограничение — 0xFFFF, но многие реализации принимают 32-bit len.
           Для совместимости порежем до 0xFFFF*2 (кратно 2) — тут оставляем len как есть. */
        drv->bdl[i].length = len;
        drv->bdl[i].flags = 0x80000000u; /* IOC bit (условный) */
        drv->pcm_bufs[i] = vptr;
        drv->pcm_len[i] = len;
    }
    drv->entries = count;

    /* Запись физического адреса BDL уже сделана в init; установим LVI (Last Valid Index) */
    if (count > 0) {
        /* LVI = count - 1 */
        nabm_writeb(drv, PO_LVI, (uint8_t)(count - 1));
        /* CIV обычно 0 */
        nabm_writeb(drv, PO_CIV, 0);
    }
    return 0;
}

/* Запустить воспроизведение */
int audio_driver_start(audio_driver_t* drv)
{
    if (!drv) return -1;
    if (drv->entries == 0) return -2;

    /* Очистим статус и запустим RUN + прерывания по окончанию буфера */
    nabm_writeb(drv, PO_SR, PO_SR_LVBCI | PO_SR_BCIS | PO_SR_DCH);
    uint8_t cr = PO_CR_RUN | PO_CR_IOCE;
    nabm_writeb(drv, PO_CR, cr);

    drv->running = 1;
    drv->paused  = 0;
    return 0;
}

/* Стоп */
int audio_driver_stop(audio_driver_t* drv)
{
    if (!drv) return -1;
    nabm_writeb(drv, PO_CR, 0); /* stop */
    drv->running = 0;
    drv->paused = 0;
    return 0;
}

/* Пауза/разпаузить (если аппарат поддерживает, иначе stop/start логика) */
int audio_driver_pause(audio_driver_t* drv, int pause)
{
    if (!drv) return -1;
    if (!drv->running) return -2;
    /* тут аппаратного бита нет в простом примере => просто запоминаем */
    drv->paused = pause ? 1 : 0;
    if (pause) {
        /* аппаратный стоп */
        nabm_writeb(drv, PO_CR, 0);
    } else {
        /* запустить снова */
        nabm_writeb(drv, PO_CR, PO_CR_RUN | PO_CR_IOCE);
    }
    return 0;
}

/* Простейшая установка громкости (значения аппарат-специфичны) */
int audio_driver_set_volume(audio_driver_t* drv, uint16_t left, uint16_t right)
{
    if (!drv) return -1;
    /* AC'97: 0x0000 = max, 0x1f1f = min (пример) — здесь просто пишем 16-bit регистр */
    uint16_t val = (left & 0xFF) | ((right & 0xFF) << 8);
    nam_writew(drv, AC97_PCM_OUT_VOL, val);
    return 0;
}

/* IRQ обработчик: вызывается ядром при IRQ устройства.
   Возвращает 1, если обработали (чтобы ядро могло ack), иначе 0.
   В обработке очищаем биты PO_SR и вызываем колбэк on_buffer_done для каждого завершённого буфера.
*/
int audio_driver_handle_irq(audio_driver_t* drv)
{
    if (!drv) return 0;
    uint8_t sr = nabm_readb(drv, PO_SR);
    if (!(sr & (PO_SR_LVBCI | PO_SR_BCIS))) {
        return 0; /* не наше или ничего интересного */
    }

    /* Очистить биты (write-1-to-clear) */
    nabm_writeb(drv, PO_SR, sr);

    /* Определим индекс завершённого буфера — это аппаратно специфично.
       В простейшем случае можно читать CIV/PIV/LVI и вычислять. */
    /* Примерный подход: прочитать CIV (Current Index Value) и LVI (Last Valid Index),
       тогда завершённый индекс = (CIV==0 ? LVI : CIV-1). */
    uint8_t civ = nabm_readb(drv, PO_CIV);
    uint8_t lvi = nabm_readb(drv, PO_LVI);
    uint16_t done_idx = (civ == 0) ? lvi : (uint16_t)(civ - 1);
    if (done_idx < drv->entries) {
        if (drv->on_buffer_done) drv->on_buffer_done(drv, done_idx);
    }

    /* Если поток завершился (по условию), можно остановить драйвер */
    /* Реальная логика: либо перезаполняем буфер, либо останавливаем. */
    return 1;
}
