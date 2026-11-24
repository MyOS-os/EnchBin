#pragma once
/* include/ench/drv.h — ABI .b_d-драйвера.
 * Каждый .b_d (reloc ELF) экспортирует символ drv_desc.
 * Ядро маршрутизирует IRQ1 (PS/2) на активный keyboard-драйвер; очередь
 * клавиш живёт в ядре (drv.c), драйверы пишут через kernel-вызовы. */
#include <stdint.h>
#include <stddef.h>

enum drv_kind {
    DRV_KEYBOARD  = 1,
    DRV_TTY_ANSI  = 2,
    DRV_FS_COMPAT = 3,   /* unix_lin, msdos */
    DRV_MISC      = 4
};

#define DRV_DESC_MAGIC 0x564E4443u /* "CDNV" ENCDRV */

struct drv_desc {
    uint32_t magic;          /* DRV_DESC_MAGIC */
    uint32_t kind;           /* enum drv_kind */
    const char *name;        /* "keyboard", "ansi", "msdos", "unix_lin" */
    const char *abi;         /* "0.1" */
    int  (*init)(void);
    int  (*ioctl)(uint64_t op, int64_t a, int64_t b, void *buf, uint64_t len);
    void (*on_irq)(uint8_t vector, uint8_t data); /* keyboard: IRQ1 байты */
};
extern struct drv_desc drv_desc;

/* ioctl-коды клавиатуры */
#define KB_IOCTL_POP   1 /* buf=int16* код клавиши (UTF-8 char или keycode) */
#define KB_IOCTL_PUSH  2 /* buf=int16* — ядро->драйвер (скан) */

/* ioctl-коды ansi */
#define ANSI_IOCTL_WRITE 1 /* buf=строка с ESC[ последовательностями */

/* ioctl-коды FS-совместимости (unix_lin / msdos) */
#define FSC_IOCTL_MAP    1 /* a=src path ptr, b=len -> включает совместимость */
#define FSC_IOCTL_LIST   2 /* buf=ench_fsc_dirent[] */
#define FSC_IOCTL_READ   3
#define FSC_IOCTL_UNAME  4 /* buf=строка */

struct ench_fsc_dirent { char name[64]; uint32_t flags; uint32_t size; };

/* очередь клавиш ядра (drv.c) */
void kq_push(int16_t key);
int  kq_pop(int16_t *out);
int  kq_wait(int16_t *out);   /* блокирующе */
void kq_wake_all(void);
