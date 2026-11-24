/* include/ench/console.h — консоль на framebuffer + J4-протокол.
 * Реализация: kernel/src/console.c (шрифт 8x8, CSI-подмножество).
 * Драйвер ansi.b_d расширяет разбор последовательностей через консольный хук. */
#ifndef ENCH_CONSOLE_H
#define ENCH_CONSOLE_H
#include <stdint.h>
#include <stddef.h>

/*цвета CSI (базовая палитра)*/
#define ENCH_C_BLACK 0x00000000u
#define ENCH_C_RED    0x00AA3737u
#define ENCH_C_GREEN  0x0037AA37u
#define ENCH_C_YELLOW 0x00AAAA37u
#define ENCH_C_BLUE   0x003737AAu
#define ENCH_C_MAGENTA 0x00AA37AAu
#define ENCH_C_CYAN   0x0037AAAAu
#define ENCH_C_WHITE  0x00C8C8C8u
#define ENCH_C_GRAY   0x006A6A6Au

void  console_init(void *fb_addr, uint32_t w, uint32_t h, uint32_t pitch, uint32_t bpp);
void  console_clear(void);
void  console_write(const char *s, size_t n);       /* с разбором CSI */
void  console_write_raw(const char *s, size_t n);   /* без CSI (для шрифтовых вызовов GUI) */
void  console_setcolor(uint32_t fg, uint32_t bg);
void  console_getcolor(uint32_t *fg, uint32_t *bg);
void  console_glyph(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void  console_fillrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);
void  console_pset(uint32_t x, uint32_t y, uint32_t rgb);
uint32_t console_width(void);   /* в символах */
uint32_t console_height(void);
void  kprintf(const char *fmt, ...);

/* ввод */
void  console_input_push(int c);   /* из драйвера клавиатуры */
int   console_input_pop(void);     /* -1 если пусто */

/* J4: владение консолью */
void  console_claim(void);         /* текущая задача занимает консоль */
void  console_release(void);
int   console_owned(void);         /* 1 если вывод ядра сейчас подавлен */
void  console_exitline(long code); /* единственная финальная строка кода */

/* расширенный CSI-хук (ставит драйвер ansi.b_d) */
typedef void (*console_ansi_hook_fn)(const char *seq, size_t len);
void  console_set_ansi_hook(console_ansi_hook_fn fn);

#endif
