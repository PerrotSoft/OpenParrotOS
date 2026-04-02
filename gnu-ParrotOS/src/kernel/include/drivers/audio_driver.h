/* audio_driver.h - минимальный аудио-драйвер (AC'97 style) */
/* Предназначен для ядра: принимает PCM-буферы и играет через DMA/BDL. */

#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <stdint.h>
#include <stddef.h>


/* Внешние примитивы, которые нужно реализовать в твоём ядре */
extern uint8_t  inb(uint16_t port);
extern void     outb(uint16_t port, uint8_t val);
extern uint16_t inw(uint16_t port);
extern void     outw(uint16_t port, uint16_t val);
extern uint32_t inl(uint16_t port);
extern void     outl(uint16_t port, uint32_t val);

/* Нужно: перевод виртуального адреса в физический (для DMA). */
extern uint32_t virt_to_phys(const void* v);

/* Максимум дескрипторов в BDL */
#define AUDIO_MAX_BDL 32

/* Структура описателя буфера для BDL (AC'97-like) */
struct audio_bdl_item {
    uint32_t addr_phys;    /* физический адрес буфера */
    uint32_t length;       /* длина в байтах (<= 0xFFFF обычно) */
    uint32_t flags;        /* IOC, reserved */
} __attribute__((packed));

/* Внутреннее состояние драйвера */
typedef struct audio_driver {
    /* I/O базы (получаем из PCI BARs) */
    uint16_t nam_base;   /* NAM (codec/mixer) I/O base */
    uint16_t nabm_base;  /* NABM (bus master) I/O base */

    /* BDL (Buffer Descriptor List) — виртуал/физик */
    struct audio_bdl_item bdl[AUDIO_MAX_BDL];
    uint32_t bdl_phys;         /* phys адрес BDL */

    /* Указатели на PCM буферы (виртуальные) */
    const void* pcm_bufs[AUDIO_MAX_BDL];
    uint32_t    pcm_len[AUDIO_MAX_BDL];
    uint16_t    entries;       /* сколько заполнено сейчас в BDL */

    /* Общие характеристики текущего потока */
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;

    /* Флаги */
    int running;
    int paused;
    int irq_vector; /* IRQ номер (если есть) */

    /* Колбэк: вызывается, когда buffer finished (можно дополнять/перезаписать) */
    void (*on_buffer_done)(struct audio_driver* drv, uint16_t idx);

    /* Дополнительные поля/поля синхронизации по необходимости... */
} audio_driver_t;

/* API */
int audio_driver_init(audio_driver_t* drv,
                      uint16_t nam_base, uint16_t nabm_base,
                      uint32_t bdl_phys_address /* phys для drv->bdl */);

int audio_driver_set_format(audio_driver_t* drv,
                            uint32_t sample_rate,
                            uint16_t channels,
                            uint16_t bits_per_sample);

int audio_driver_prepare_play(audio_driver_t* drv,
                              const void* pcm_ptrs[], const uint32_t lengths[], uint16_t count);

int audio_driver_start(audio_driver_t* drv);
int audio_driver_stop(audio_driver_t* drv);
int audio_driver_pause(audio_driver_t* drv, int pause);
int audio_driver_set_volume(audio_driver_t* drv, uint16_t left, uint16_t right);

/* Вызывать из IRQ: вернуть >0 если обработали */
int audio_driver_handle_irq(audio_driver_t* drv);

#endif /* AUDIO_DRIVER_H */
