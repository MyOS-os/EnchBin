// userspace/appscore/myos/src/gfx.c — MyOS: примитивы рисования (ring 0).
// Захватывает framebuffer ядра (ench_boot_fb), пишет пиксели напрямую;
// текст — шрифтом 3x5 ядра (kfont_glyph, cell 4*scale x 6*scale).
// Символы gfx_* видят sibling-юниты (gui/desktop/taskman) через symtab
// единицы после ld -r.
#include <ench/kernel.h>
#include <ench/myos.h>

static u32 *gfb;
static int GW, GH, GP;

void gfx_init(void) {
    struct limine_framebuffer *f = ench_boot_fb();
    if (!f) { gfb = 0; return; }
    gfb = f->address;
    GW = (int)f->width;
    GH = (int)f->height;
    GP = (int)(f->pitch / 4);
    if (GP < GW) GP = GW;
}

void gfx_pixel(int x, int y, u32 c) {
    if (!gfb || x < 0 || y < 0 || x >= GW || y >= GH) return;
    gfb[y * GP + x] = c;
}

void gfx_rect(int x, int y, int w, int h, u32 c, int fill) {
    if (!fill) {
        for (int i = 0; i < w; i++) { gfx_pixel(x + i, y, c); gfx_pixel(x + i, y + h - 1, c); }
        for (int j = 0; j < h; j++) { gfx_pixel(x, y + j, c); gfx_pixel(x + w - 1, y + j, c); }
        return;
    }
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            gfx_pixel(x + i, y + j, c);
}

void gfx_line(int x0, int y0, int x1, int y1, u32 c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

// символ шрифтом 3x5 (kfont_glyph): 3 строки бит на строку, cell 4*scale x 6*scale
void gfx_char(int x, int y, char ch, u32 c, int scale) {
    if (scale < 1) scale = 1;
    u8 rows[5];
    kfont_glyph(ch, rows);
    for (int py = 0; py < 5; py++)
        for (int gx = 0; gx < 3; gx++)
            if (rows[py] & (4 >> gx))
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        gfx_pixel(x + gx * scale + sx, y + py * scale + sy, c);
}

void gfx_text(int x, int y, const char *s, u32 c, int scale) {
    if (scale < 1) scale = 1;
    for (usize i = 0; s[i]; i++)
        gfx_char(x + (int)i * 4 * scale, y, s[i], c, scale);
}

void gfx_clear(u32 c) {
    if (!gfb) return;
    for (int i = 0; i < GW * GH; i++) gfb[i] = c;
}

void gfx_flush(void) { /* примитивы пишутся сразу; резерв под батчинг */ }

int gfx_width(void)  { return GW; }
int gfx_height(void) { return GH; }
