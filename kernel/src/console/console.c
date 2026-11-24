// kernel/src/console/console.c — текстовая консоль на framebuffer (шрифт 3x5)
// и протокол J4: монопольный вывод приложения + последняя строка "exit N".
#include <ench/kernel.h>
#include <ench/osstrace.h>

extern const u8 ench_font3x5[96][5]; // kernel/src/console/font.c

#define CELL_W 4
#define CELL_H 6

static u32 *fb; static int W, H, P;      // P в пикселях (u32)
static int cx, cy, cols, rows;
static u8 cfg = ENCH_LGRAY, cbg = ENCH_BLACK;
static void *owner;                       // J4: владелец экрана

static u16 colors[16] = {
    [ENCH_BLACK]   = 0x0000, [ENCH_BLUE]    = 0x8000, [ENCH_GREEN]   = 0x0080, [ENCH_CYAN]    = 0x8080,
    [ENCH_RED]     = 0x0015, [ENCH_MAGENTA] = 0x8015, [ENCH_BROWN]   = 0x4015, [ENCH_LGRAY]   = 0x6060,
    [ENCH_DGRAY]   = 0x1818, [ENCH_LBLUE]   = 0xC000, [ENCH_LGREEN]  = 0x00C0, [ENCH_LCYAN]   = 0xC0C0,
    [ENCH_LRED]    = 0x0040, [ENCH_LMAGENTA]= 0xC040, [ENCH_YELLOW]  = 0x80C0, [ENCH_WHITE]   = 0xC0C0,
};

// ЦВЕТ пикселя: fg -> белый оттенок по атрибуту, bg -> тёмный (toy-палитра)
static u32 fg_color(u8 a) {
    u16 c = colors[a & 15];
    return myos_rgb((c >> 8) & 0xC0 | 0x20, (c >> 4) & 0xC0 | 0x20, (c & 0xC0) | 0x20);
}
static u32 bg_color(u8 a) {
    u16 c = colors[a & 15];
    return myos_rgb((c >> 8) & 0x30, (c >> 4) & 0x30, c & 0x30);
}

void kconsole_init(void) {
    struct limine_framebuffer *f = ench_boot_fb();
    if (!f) return;
    fb = f->address;
    W = (int)f->width; H = (int)f->height;
    P = (int)(f->pitch / 4);
    cols = W / CELL_W; rows = H / CELL_H;
    cx = cy = 0;
    kclear();
}

void ksetattr(u8 fg, u8 bg) { cfg = fg; cbg = bg; }

static void cell_clear(int x, int y) {
    u32 c = bg_color(cbg);
    for (int py = 0; py < CELL_H; py++)
        for (int px = 0; px < CELL_W; px++)
            fb[(y * CELL_H + py) * P + x * CELL_W + px] = c;
}

static void cell_draw(int x, int y, char ch) {
    u8 rows[5];
    kfont_glyph(ch, rows);
    u32 f = fg_color(cfg), b = bg_color(cbg);
    for (int py = 0; py < CELL_H; py++) {
        u32 *line = fb + (y * CELL_H + py) * P + x * CELL_W;
        for (int px = 0; px < CELL_W; px++) line[px] = b;
        if (py < 5)
            for (int gx = 0; gx < 3; gx++)
                if (rows[py] & (4 >> gx)) line[gx] = f;
    }
}

void kscroll(int n) {
    if (!fb) return;
    if (n > rows) n = rows;
    int shift = n * CELL_H;
    for (int y = 0; y < H - shift; y++)
        ench_memcpy(fb + y * P, fb + (y + shift) * P, (usize)P * 4);
    for (int y = H - shift; y < H; y++)
        for (int x = 0; x < W; x++) fb[y * P + x] = bg_color(cbg);
    cy -= n;
}

void kclear(void) {
    if (!fb) return;
    u32 c = bg_color(cbg);
    for (int i = 0; i < W * H; i++) fb[i] = c;
    cx = cy = 0;
}

static void newline(void) {
    cx = 0;
    if (++cy >= rows) kscroll(1);
}

static void raw_putc(char c) {
    if (!fb) return;
    if (c == '\n') { newline(); return; }
    if (c == '\r') { cx = 0; return; }
    if (c == '\t') { raw_putc(' '); raw_putc(' '); return; }
    if (cx >= cols) newline();
    cell_draw(cx, cy, c);
    cx++;
}

// Экран без учёта J4-владельца (ядро: exit-строка, паники)
static void raw_write(const char *s, usize n) { for (usize i = 0; i < n; i++) raw_putc(s[i]); }

void kputc(char c) {
    char b[1] = { c };
    kserial_write(b, 1);
    if (owner) return;              // J4: приложение монопольно, лог -> COM1
    raw_write(b, 1);
}

void kputs(const char *s) {
    kserial_write(s, ench_strlen(s));
    if (owner) return;
    raw_write(s, ench_strlen(s));
}

void kprintf(const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    kvformat(buf, sizeof buf, fmt, ap);
    va_end(ap);
    kputs(buf);
}

// J4-строка выхода: последняя строка, отдельно на новой строке.
void console_exit_line(i32 code) {
    char b[32];
    int n = kvformat(b, sizeof b, "\n" ENCH_EXIT_LINE_FMT "\n", (va_list){&code});
    (void)n;
    raw_write(b, ench_strlen(b));
}

void console_acquire(void *task) { owner = task; }
void console_release(void *task) { if (owner == task) owner = 0; }

void kfont_glyph(char ch, u8 rows[5]) {
    u8 idx = (u8)ch;
    if (idx < 0x20 || idx > 0x7F) idx = '?';
    if (idx >= 0x20 && idx <= 0x7F)
        for (int i = 0; i < 5; i++) rows[i] = ench_font3x5[idx - 0x20][i];
}
