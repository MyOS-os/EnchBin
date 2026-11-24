// userspace/appscore/myos/src/fileman.c — MyOS файловый менеджер (ring 0, ET_REL "main").
// Листинг каталога (fs_listdir), навигация: 1..9 вход в каталог,
// b — на уровень вверх, r — обновить, q — выход.
#include <ench/kernel.h>
#include <ench/myos.h>
#include <ench/task.h>

#define LIST_N 9

static char cwd[96] = "/";

static int is_dirname(const char *base) {
    char path[128];
    fs_path_join(path, sizeof path, cwd, base);
    struct ench_fstat st;
    if (fs_stat(path, &st) != ENCH_EOK) return 0;
    return st.is_dir;
}

static void draw_listing(void) {
    static char buf[512];
    i32 n = fs_listdir(cwd, buf, sizeof buf, 0);
    gfx_rect(4, 12, 200, 90, MYOS_BG, 1);
    gfx_text(6, 12, cwd, MYOS_ACCENT, 1);
    usize i = 0;
    for (i32 row = 0; row < LIST_N && i < sizeof buf && buf[i]; row++) {
        char name[64];
        usize l = 0;
        while (buf[i] && buf[i] != '\n' && l + 1 < sizeof name) name[l++] = buf[i++];
        if (buf[i] == '\n') i++;
        name[l] = 0;
        if (!l) continue;
        char line[80];
        usize o = 0;
        line[o++] = (char)('1' + row);
        line[o++] = ' ';
        for (usize j = 0; name[j] && o < 40; j++) line[o++] = name[j];
        if (is_dirname(name) && o < 42) { line[o++] = '/'; }
        line[o] = 0;
        gfx_text(6, 12 + (row + 1) * 7, line, is_dirname(name) ? MYOS_OK : MYOS_TEXT, 1);
    }
    if (n < 0) gfx_text(6, 20, "(oshibka listdir)", MYOS_ERR, 1);
    gfx_text(6, 12 + (LIST_N + 1) * 7, "1..9-vhod b-up r-obl q-vyhod", MYOS_TEXT, 1);
}

static int nth_name(int idx, char *out, usize cap) {
    static char buf[512];
    (void)cap;
    if (fs_listdir(cwd, buf, sizeof buf, 0) < 0) return 0;
    usize i = 0;
    for (int row = 0; i < sizeof buf && buf[i]; row++) {
        usize l = 0;
        while (buf[i] && buf[i] != '\n' && l + 1 < 64) out[l++] = buf[i++];
        if (buf[i] == '\n') i++;
        out[l] = 0;
        if (row == idx) return 1;
    }
    return 0;
}

void main(const char *args) {
    (void)args;
    if (args && args[0]) {
        usize l = ench_strlen(args);
        if (l < sizeof cwd) ench_memcpy(cwd, args, l + 1);
    }
    gfx_init();
    gui_desktop();
    i32 w = gui_win_create("fileman", 4, 8, 210, 110, MYOS_BG);
    gui_win_focus(w);
    draw_listing();
    for (;;) {
        i32 k = gui_poll_key();
        if (k < 0) { task_yield(); continue; }
        if (k == 'q' || k == 27) break;
        if (k == 'r') { draw_listing(); continue; }
        if (k == 'b') {
            if (cwd[1]) {
                usize l = ench_strlen(cwd);
                while (l > 1 && cwd[l - 1] != '/') l--;
                if (l > 1) cwd[l - 1] = 0;
                else cwd[1] = 0;
            }
            draw_listing();
            continue;
        }
        if (k >= '1' && k <= '9') {
            char name[64];
            if (!nth_name(k - '1', name, sizeof name)) continue;
            if (!is_dirname(name)) continue;
            char np[128];
            fs_path_join(np, sizeof np, cwd, name);
            usize l = ench_strlen(np);
            if (l < sizeof cwd) ench_memcpy(cwd, np, l + 1);
            draw_listing();
        }
    }
    gui_win_close(w);
}
