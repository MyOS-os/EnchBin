#pragma once
/* include/ench/console.h — framebuffer-консоль ядра + J4-протокол.
 * Шрифт — компактный встроенный 5x7 (заменяем, см. drivers/ansi). */
#include <stdint.h>
#include <stddef.h>

struct ench_fb {
    void    *addr;   /* HHDM-адрес фреймбуфера */
    uint32_t w, h, pitch, bpp;
    uint32_t cw, ch; /* ширина/высота символа в пикселях */
};
extern struct ench_fb ench_fb;

void console_init(void *fb_addr, uint32_t w, uint32_t h, uint32_t pitch, uint32_t bpp);
void console_clear(void);
void console_putc(char c);
void console_write(const char *s, size_t n);
void console_set_fg(uint32_t rgb);
void console_set_bg(uint32_t rgb);
void console_scroll(void);
void console_cursor_home(void);

/* строка с ANSI-подобными кодами \e[..m (обслуживает drivers/ansi) */
void console_write_ansi(const char *s, size_t n);

/* kprintf ядра: попадает в консоль ТОЛЬКО когда консоль не занята приложением
 * (J4) либо когда включён OssTrace */
void kprintf(const char *fmt, ...);

/* J4: приложение занимает консоль; сообщения ядра подавляются */
void console_claim(int64_t pid);
void console_release(void);
int  console_claimed(void);
/* финальная строка: "exit code: N\n" — всегда видна */
void console_exit_line(int64_t code);

/* отрисовка символа шрифтом в произвольную позицию (для GUI Platform/MyOS) */
void console_glyph(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg, uint32_t scale);
