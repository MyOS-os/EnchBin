// include/ench/myos.h — API графической среды MyOS (ring 0, Platform/MyOS).
// MyOS = UI (gui, gfx) + системные программы (taskman, fileman, accmgr).
#pragma once

#include <ench/base.h>

// ---- цвета (framebuffer ABGR little-endian) ----
static inline u32 myos_rgb(u8 r, u8 g, u8 b) { return 0xFF000000u | ((u32)r << 16) | ((u32)g << 8) | b; }
#define MYOS_BLACK   myos_rgb(12,12,16)
#define MYOS_BG      myos_rgb(24,26,34)
#define MYOS_PANEL   myos_rgb(34,38,48)
#define MYOS_TEXT    myos_rgb(220,224,235)
#define MYOS_ACCENT  myos_rgb(96,165,250)
#define MYOS_OK      myos_rgb(74,222,128)
#define MYOS_WARN    myos_rgb(250,204,21)
#define MYOS_ERR     myos_rgb(248,113,113)

// ---- gfx (userspace/appscore/myos/src/gfx.c) ----
void gfx_init(void);                    // захват framebuffer
void gfx_pixel(int x, int y, u32 c);
void gfx_rect(int x, int y, int w, int h, u32 c, int fill);
void gfx_line(int x0, int y0, int x1, int y1, u32 c);
// Текст шрифтом 3x5 ядра (kfont_glyph); scale >= 1 (cell 4*scale x 6*scale)
void gfx_char(int x, int y, char ch, u32 c, int scale);
void gfx_text(int x, int y, const char *s, u32 c, int scale);
void gfx_clear(u32 c);
void gfx_flush(void);                   // примитивы пишутся сразу; flush — резерв
int  gfx_width(void);
int  gfx_height(void);

// ---- gui (userspace/appscore/myos/src/gui.c) ----
struct myos_win {
    i32 id;
    i32 x, y, w, h;
    u32 bg;
    char title[24];
    int focused;
};

i32  gui_win_create(const char *title, int x, int y, int w, int h, u32 bg);
void gui_win_close(i32 id);
void gui_win_draw(i32 id);              // рамка, заголовок, кнопки окна
void gui_win_focus(i32 id);
i32  gui_focus(void);
void gui_btn_add(i32 win, const char *label, int x, int y, int w, int h);
void gui_btn_draw(i32 win);
i32  gui_btn_hit(i32 win, int x, int y); // id кнопки или -1
// Опрос клавиатуры через драйвер input; -1 если пусто (ASCII или сканкод)
i32  gui_poll_key(void);
void gui_desktop(void);                 // фон рабочего стола
