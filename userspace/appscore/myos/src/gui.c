// userspace/appscore/myos/src/gui.c — MyOS: окна и кнопки (ring 0).
// Текстовый UI поверх gfx (шрифт 3x5, scale 1: символ 3x5 в cell 4x6).
// Кнопки — прямоугольники; опрос клавиатуры — очередь ядра (kinput_pop).
// main() отсутствует: это библиотека единицы; main в desktop/taskman/...
#include <ench/kernel.h>
#include <ench/myos.h>

#define WIN_MAX 8
#define BTN_MAX 6
#define TITLE_H 6

struct myos_btn { int used; char label[12]; i32 x, y, w, h; };

static struct myos_win wins[WIN_MAX];
static struct myos_btn btns[WIN_MAX][BTN_MAX];
static i32 focused_win = -1;

i32 gui_win_create(const char *title, int x, int y, int w, int h, u32 bg) {
    for (int i = 0; i < WIN_MAX; i++) {
        if (wins[i].id) continue;
        wins[i].id = i + 1;
        wins[i].x = x; wins[i].y = y; wins[i].w = w; wins[i].h = h;
        wins[i].bg = bg;
        wins[i].focused = 0;
        for (usize j = 0; title[j] && j < sizeof wins[i].title - 1; j++)
            wins[i].title[j] = title[j];
        wins[i].title[sizeof wins[i].title - 1] = 0;
        for (int b = 0; b < BTN_MAX; b++) btns[i][b].used = 0;
        return wins[i].id;
    }
    return -1;
}

void gui_win_close(i32 id) {
    if (id < 1 || id > WIN_MAX) return;
    wins[id - 1].id = 0;
    if (focused_win == id) focused_win = -1;
}

static void draw_frame(struct myos_win *w, u32 frame) {
    gfx_rect(w->x, w->y, w->w, w->h, w->bg, 1);
    gfx_rect(w->x, w->y, w->w, TITLE_H, frame, 1);
    gfx_rect(w->x, w->y, w->w, w->h, frame, 0);
    gfx_text(w->x + 4, w->y + 1, w->title, MYOS_BLACK, 1);
}

void gui_win_draw(i32 id) {
    if (id < 1 || id > WIN_MAX || !wins[id - 1].id) return;
    struct myos_win *w = &wins[id - 1];
    draw_frame(w, w->focused ? MYOS_ACCENT : MYOS_PANEL);
    gui_btn_draw(id);
}

void gui_win_focus(i32 id) {
    if (id < 1 || id > WIN_MAX) return;
    for (int i = 0; i < WIN_MAX; i++) wins[i].focused = 0;
    if (wins[id - 1].id) { wins[id - 1].focused = 1; focused_win = id; }
}

i32 gui_focus(void) { return focused_win; }

void gui_btn_add(i32 win, const char *label, int x, int y, int w, int h) {
    if (win < 1 || win > WIN_MAX) return;
    for (int b = 0; b < BTN_MAX; b++) {
        if (btns[win - 1][b].used) continue;
        btns[win - 1][b].used = 1;
        btns[win - 1][b].x = x; btns[win - 1][b].y = y;
        btns[win - 1][b].w = w; btns[win - 1][b].h = h;
        for (usize i = 0; label[i] && i < sizeof btns[0][0].label - 1; i++)
            btns[win - 1][b].label[i] = label[i];
        btns[win - 1][b].label[sizeof btns[0][0].label - 1] = 0;
        return;
    }
}

void gui_btn_draw(i32 win) {
    if (win < 1 || win > WIN_MAX) return;
    for (int b = 0; b < BTN_MAX; b++) {
        struct myos_btn *t = &btns[win - 1][b];
        if (!t->used) continue;
        int ax = wins[win - 1].x + t->x;
        int ay = wins[win - 1].y + t->y;
        gfx_rect(ax, ay, t->w, t->h, MYOS_PANEL, 1);
        gfx_rect(ax, ay, t->w, t->h, MYOS_TEXT, 0);
        int tx = ax + 2, ty = ay + t->h / 2 - 2;
        gfx_text(tx, ty, t->label, MYOS_TEXT, 1);
    }
}

i32 gui_btn_hit(i32 win, int x, int y) {
    if (win < 1 || win > WIN_MAX) return -1;
    for (int b = 0; b < BTN_MAX; b++) {
        struct myos_btn *t = &btns[win - 1][b];
        if (!t->used) continue;
        int ax = wins[win - 1].x + t->x;
        int ay = wins[win - 1].y + t->y;
        if (x >= ax && x < ax + t->w && y >= ay && y < ay + t->h) return b;
    }
    return -1;
}

// клавиатура через очередь ядра: ASCII или -1
i32 gui_poll_key(void) {
    char c;
    if (!kinput_pop(&c, 0)) return -1;
    return (u8)c;
}

void gui_desktop(void) {
    gfx_init();
    gfx_clear(MYOS_BG);
    // верхняя панель
    gfx_rect(0, 0, gfx_width(), 8, MYOS_PANEL, 1);
    gfx_text(4, 1, "MyOS 0.1", MYOS_TEXT, 1);
}
