// userspace/appscore/fsprobe/src/fsprobe.c — ring0-приложение (ET_REL "main").
// Инспекция ramfs: stat ключевых путей, правила имён (hidden/lock),
// листинг корня и /Platform. Демонстрирует ENCH_ELOCKED на .pass.__.
#include <ench/kernel.h>

static void out(const char *s) { console_app_write(s, ench_strlen(s)); }
static void outln(const char *s) { out(s); out("\n"); }

static void stat_line(const char *path) {
    struct ench_fstat st;
    char buf[160];
    usize o = 0;
    for (usize i = 0; path[i] && o < 100; i++) buf[o++] = path[i];
    for (usize i = 0; ": " [i]; i++) buf[o++] = ": " [i];
    i32 r = fs_stat(path, &st);
    if (r != ENCH_EOK) {
        const char *m = r == ENCH_ENOENT ? "net" : "oshibka";
        for (usize i = 0; m[i]; i++) buf[o++] = m[i];
        buf[o++] = '\n';
        console_app_write(buf, o);
        return;
    }
    if (st.is_dir) { out(buf); outln("katalog"); return; }
    // размер
    char num[24];
    usize n = 0;
    u64 u = (u64)st.size;
    if (!u) num[n++] = '0';
    while (u) { num[n++] = (char)('0' + u % 10); u /= 10; }
    for (usize i = 0; i < n; i++) buf[o++] = num[n - 1 - i];
    buf[o++] = ' ';
    buf[o++] = 'b';
    if (st.locked) { for (usize i = 0; " [lock]"[i]; i++) buf[o++] = " [lock]" [i]; }
    if (st.hidden) { for (usize i = 0; " [hidden]"[i]; i++) buf[o++] = " [hidden]" [i]; }
    buf[o++] = '\n';
    console_app_write(buf, o);
}

static void dump_dir(const char *path, int show_hidden) {
    static char lst[512];
    i32 n = fs_listdir(path, lst, sizeof lst, show_hidden);
    out(path);
    outln(":");
    if (n < 0) { outln("  (ne katalog)"); return; }
    usize i = 0;
    while (i < sizeof lst && lst[i]) {
        out("  ");
        char one[64];
        usize l = 0;
        while (lst[i] && lst[i] != '\n' && l + 1 < sizeof one) one[l++] = lst[i++];
        if (lst[i] == '\n') i++;
        one[l] = 0;
        if (l) outln(one);
    }
    outln("");
}

void main(const char *args) {
    (void)args;
    outln("--- fsprobe ---");
    stat_line("/Platform");
    stat_line("/Platform/Apps");
    stat_line("/User/apps");
    stat_line(ENCH_INITD_DIR "/main.einit");
    stat_line(ENCH_ACC_DIR "/root/" ENCH_PASSFILE);

    outln("--- pravila imen ---");
    out("name_hidden(\".pass.__\") = ");
    outln(fs_name_hidden(ENCH_PASSFILE) ? "1" : "0");
    out("name_locked(\".pass.__\") = ");
    outln(fs_name_locked(ENCH_PASSFILE) ? "1" : "0");

    outln("--- koren ---");
    dump_dir("/", 0);
    outln("--- /Platform ---");
    dump_dir("/Platform", 0);

    // демонстрация lock: попытка чтения .pass.__ -> ENCH_ELOCKED
    i32 fd = fs_open(ENCH_ACC_DIR "/root/" ENCH_PASSFILE, ENCH_O_READ);
    if (fd == ENCH_ELOCKED) outln("lock-rules: chtenie .pass.__ otkloneno (ELOCKED) OK");
    else if (fd < 0) outln("lock-rules: .pass.__ ne naiden (akkauntov poka net)");
    else { fs_close(fd); outln("lock-rules: VNIMANIE .pass.__ chitaetsya!"); }
}
