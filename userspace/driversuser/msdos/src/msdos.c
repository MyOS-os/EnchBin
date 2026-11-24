// userspace/driversuser/msdos/src/msdos.c — пользовательский драйвер msdos
// (.b_d, каталог userd, тип compat). Демонстрация userd-драйвера:
// листинг каталога в стиле MS-DOS 8.3 (верхний регистр, обрезка имени/расширения).
// ioctl: 1 = DIR83 — arg = &struct ench_dir83_req { const char *path;
//        char *out; usize cap; } — заполняет out "NAME.EXT\n", возвращает число
//        записей или err.
#include <ench/kernel.h>

struct ench_dir83_req { const char *path; char *out; usize cap; };

static void to83(const char *name, char *out83) {
    usize o = 0;
    const char *dot = 0;
    for (const char *p = name; *p; p++)
        if (*p == '.') { dot = p; break; }
    usize bl = dot ? (usize)(dot - name) : ench_strlen(name);
    for (usize i = 0; i < bl && o < 8; i++) {
        char c = name[i];
        out83[o++] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    if (dot) {
        usize el = ench_strlen(dot + 1);
        if (el > 3) el = 3;
        if (o < 8) out83[o++] = '.';
        for (usize i = 0; i < el && o < 12; i++) {
            char c = dot[1 + i];
            out83[o++] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
        }
    }
    out83[o] = 0;
}

static int msdos_ioctl(u32 cmd, u64 arg) {
    if (cmd != 1) return ENCH_EINVAL;
    struct ench_dir83_req *r = (struct ench_dir83_req *)arg;
    if (!r || !r->out || !r->cap) return ENCH_EINVAL;
    char list[512];
    i32 n = fs_listdir(r->path ? r->path : "/", list, sizeof list, 0);
    if (n < 0) return n;
    usize o = 0;
    usize i = 0;
    while (i < sizeof list && list[i]) {
        char name[64];
        usize l = 0;
        while (list[i] && list[i] != '\n' && l + 1 < sizeof name) name[l++] = list[i++];
        if (list[i] == '\n') i++;
        name[l] = 0;
        if (!l) continue;
        char s83[14];
        to83(name, s83);
        for (usize j = 0; s83[j] && o + 1 < r->cap; j++) r->out[o++] = s83[j];
        if (o + 1 < r->cap) r->out[o++] = '\n';
    }
    if (o < r->cap) r->out[o] = 0;
    return n;
}

static struct ench_drv msdos_drv = {
    .name = "msdos", .type = "compat",
    .init = 0, .write = 0, .read = 0, .ioctl = msdos_ioctl,
};

int ench_drv_init(void) {
    return drv_register(&msdos_drv);
}
