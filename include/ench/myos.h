/* include/ench/myos.h — ядро GUI-замашек EnchBin (kernel/src/myos.c).
 * Platform/MyOS: ring0-приложения gui и gfx рисуют через эти вызовы.
 * Разделение: ядро (myos.c + console) и пользовательский код (gui.c/gfx.c). */
#ifndef ENCH_MYOS_H
#define ENCH_MYOS_H
#include <stdint.h>

struct ench_wininfo { long id; int32_t x, y, w, h; char title[24]; int focused; };

long myos_win_open(int32_t x, int32_t y, int32_t w, int32_t h, const char *title);
void myos_win_close(long id);
void myos_win_move(long id, int32_t dx, int32_t dy);
void myos_win_focus_next(void);
long myos_win_focused(void);
void myos_btn_add(long win, const char *label, long btn_id);
void myos_redraw(void);

/* события: клавиши/кнопки окна в фокусе; -1 если пусто */
long myos_poll_event(long win, long *btn_id);

/* примитивы */
void myos_gfx_clear(uint32_t rgb);
void myos_gfx_fillrect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb);
void myos_gfx_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb);
void myos_gfx_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rgb);
void myos_gfx_circle(int32_t cx, int32_t cy, int32_t r, uint32_t rgb, int fill);
void myos_gfx_text(const char *s, int32_t x, int32_t y, uint32_t rgb);

/* команды SYS_gui_draw */
#define GUI_CLEAR    1
#define GUI_FILLRECT 2
#define GUI_RECT     3
#define GUI_LINE     4
#define GUI_TEXT     5 /* c=ptr на строку (user copy), d=color */
#define GUI_CIRCLE   6

#endif
