// drivers/ansi/src/ansi.c — .b_d term-драйвер ANSI CSI.
// write(): обычный текст идёт на экран (console_app_write), а последовательности
// ESC[<n>;<m><final> обрабатываются: 2J — очистка, m — цвет (0m сброс,
// 30-37/90-97 текст, 40-47 фон; H/K без экранных API ядра — пропуск).
// ioctl: 1 = очистка, 2 = setattr (arg = fg | bg<<8, атрибуты kernel.h).
#include <ench/kernel.h>

static u8 csi_fg = ENCH_LGRAY, csi_bg = ENCH_BLACK;

static const u8 fg_csi[8] = {
    ENCH_BLACK, ENCH_RED, ENCH_GREEN, ENCH_BROWN,
    ENCH_BLUE, ENCH_MAGENTA, ENCH_CYAN, ENCH_LGRAY
};
static const u8 fg_csi_hi[8] = {
    ENCH_DGRAY, ENCH_LRED, ENCH_LGREEN, ENCH_YELLOW,
    ENCH_LBLUE, ENCH_LMAGENTA, ENCH_LCYAN, ENCH_WHITE
};
static const u8 bg_csi[8] = {
    ENCH_BLACK, ENCH_BLUE, ENCH_GREEN, ENCH_CYAN,
    ENCH_RED, ENCH_MAGENTA, ENCH_BROWN, ENCH_LGRAY
};

static int ansi_write(const void *buf, usize len) {
    const char *s = (const char *)buf;
    usize i = 0;
    while (i < len) {
        if (s[i] == 0x1B && i + 1 < len && s[i + 1] == '[') {
            i += 2;
            u32 n1 = 0;
            while (i < len && s[i] >= '0' && s[i] <= '9') {
                n1 = n1 * 10 + (u32)(s[i] - '0');
                i++;
            }
            if (i >= len) break;              // недописанная CSI — до следующего write
            char f = s[i++];
            if (f == 'J' && n1 == 2) {
                kclear();
            } else if (f == 'm') {
                if (n1 == 0) { csi_fg = ENCH_LGRAY; csi_bg = ENCH_BLACK; ksetattr(csi_fg, csi_bg); }
                else if (n1 >= 30 && n1 <= 37) { csi_fg = fg_csi[n1 - 30]; ksetattr(csi_fg, csi_bg); }
                else if (n1 >= 90 && n1 <= 97) { csi_fg = fg_csi_hi[n1 - 90]; ksetattr(csi_fg, csi_bg); }
                else if (n1 >= 40 && n1 <= 47) { csi_bg = bg_csi[n1 - 40]; ksetattr(csi_fg, csi_bg); }
            }
            // 'H'/'K'/'A'..'D' — в v0.1 без экранных API ядра: пропускаем
            continue;
        }
        console_app_write(&s[i], 1);
        i++;
    }
    return (int)len;
}

static int ansi_ioctl(u32 cmd, u64 arg) {
    switch (cmd) {
    case 1:
        kclear();
        return ENCH_EOK;
    case 2:
        csi_fg = (u8)(arg & 0xFF);
        csi_bg = (u8)((arg >> 8) & 0xFF);
        ksetattr(csi_fg, csi_bg);
        return ENCH_EOK;
    default:
        return ENCH_EINVAL;
    }
}

static struct ench_drv ansi_drv = {
    .name = "ansi", .type = "term",
    .init = 0, .write = ansi_write, .read = 0, .ioctl = ansi_ioctl,
};

int ench_drv_init(void) {
    return drv_register(&ansi_drv);
}
