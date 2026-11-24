// userspace/appscore/myos/src/taskman.c — MyOS диспетчер задач (ring 0, ET_REL "main").
// Таблица задач (task_list из ядра), обновление 'r', выход 'q'.
#include <ench/kernel.h>
#include <ench/myos.h>
#include <ench/task.h>

static const char *state_name(u8 s) {
    switch (s) {
    case ENCH_TASK_RUN:   return "RUN";
    case ENCH_TASK_READY: return "RDY";
    case ENCH_TASK_SLEEP: return "SLP";
    case ENCH_TASK_BLOCK: return "BLK";
    case ENCH_TASK_DEAD:  return "DED";
    default:              return "?";
    }
}

// целое в строку (десятичное)
static int put_i(char *b, i32 v) {
    char t[12]; usize n = 0;
    u32 u = v < 0 ? (u32)(-v) : (u32)v;
    if (!u) t[n++] = '0';
    while (u) { t[n++] = (char)('0' + u % 10); u /= 10; }
    usize k = 0;
    if (v < 0) b[k++] = '-';
    while (n) b[k++] = t[--n];
    b[k] = 0;
    return (int)k;
}

static void pad(char *b, int *len, int width) {
    while (*len < width) b[(*len)++] = ' ';
    b[*len] = 0;
}

static void draw_table(void) {
    static struct ench_taskinfo ti[ENCH_TASK_MAX];
    i32 n = task_list(ti, ENCH_TASK_MAX);
    gfx_rect(4, 12, 190, 70, MYOS_BG, 1);
    gfx_text(6, 12, "PID  NAME    ST  RT  W", MYOS_ACCENT, 1);
    for (i32 i = 0; i < n && i < 9; i++) {
        char line[64];
        int o = 0;
        o += put_i(line + o, ti[i].pid);
        pad(line, &o, 5);
        usize j = 0;
        for (; ti[i].name[j] && j < 8; j++) line[o++] = ti[i].name[j];
        pad(line, &o, 14);
        const char *st = state_name(ti[i].state);
        for (j = 0; st[j]; j++) line[o++] = st[j];
        pad(line, &o, 18);
        o += put_i(line + o, ti[i].rt ? 1 : 0);
        pad(line, &o, 20);
        o += put_i(line + o, (i32)ti[i].weight);
        line[o] = 0;
        gfx_text(6, 12 + (i + 1) * 7, line, MYOS_TEXT, 1);
    }
    char cnt[24];
    int o = 0;
    const char *pre = "zadach: ";
    for (usize j = 0; pre[j]; j++) cnt[o++] = pre[j];
    o += put_i(cnt + o, n);
    cnt[o] = 0;
    gfx_text(6, 12 + 10 * 7 + 2, cnt, MYOS_WARN, 1);
}

void main(const char *args) {
    (void)args;
    gfx_init();
    gui_desktop();
    i32 w = gui_win_create("taskman", 4, 8, 200, 100, MYOS_BG);
    gui_win_focus(w);
    draw_table();
    gfx_text(6, 96, "r - obnovit, q - vyhod", MYOS_TEXT, 1);
    for (;;) {
        i32 k = gui_poll_key();
        if (k < 0) { task_yield(); continue; }
        if (k == 'q' || k == 27) break;
        if (k == 'r') draw_table();
    }
    gui_win_close(w);
}
