// userspace/appscore/myos/src/accmgr.c — MyOS менеджер аккаунтов (ring 0, ET_REL "main").
// Показ текущего пользователя и списка аккаунтов; 1 — сменить пользователя
// (логин+пароль с клавиатуры), 2 — создать аккаунт, q — выход.
#include <ench/kernel.h>
#include <ench/myos.h>
#include <ench/task.h>

static void put_text(int x, int y, const char *s, u32 c) { gfx_text(x, y, s, c, 1); }

// чтение строки с клавиатуры (echo: пароль — звёздочки)
static int read_line(char *buf, int cap, int echo) {
    int n = 0;
    for (;;) {
        char c;
        if (!kinput_pop(&c, 1)) return -1;
        if (c == '\n') { buf[n] = 0; return n; }
        if (c == '\b' || c == 0x7F) {
            if (n) {
                n--;
                console_app_write("\b \b", 3);
            }
            continue;
        }
        if (c < 0x20 || (u8)c > 0x7E) continue;
        if (n + 1 >= cap) continue;
        buf[n++] = c;
        char star = echo ? c : '*';
        console_app_write(&star, 1);
    }
}

static void draw(void) {
    gfx_rect(4, 12, 210, 80, MYOS_BG, 1);
    const char *cur = acc_current();
    put_text(6, 12, cur ? cur : "(ne voshli)", MYOS_OK);
    static char list[384];
    i32 n = acc_list(list, sizeof list);
    usize i = 0;
    for (int row = 0; row < 6 && i < sizeof list && list[i]; row++) {
        char name[48];
        usize l = 0;
        while (list[i] && list[i] != '\n' && l + 1 < sizeof name) name[l++] = list[i++];
        if (list[i] == '\n') i++;
        name[l] = 0;
        if (!l) continue;
        put_text(6, 12 + (row + 1) * 7, name, MYOS_TEXT);
    }
    put_text(6, 12 + 7 * 7 + 2, "1-vhod 2-sozdat r-obl q-vyhod", MYOS_TEXT);
    (void)n;
}

static void do_login(void) {
    char login[32], pass[40];
    console_app_write("login: ", 7);
    if (read_line(login, sizeof login, 1) < 0) return;
    console_app_write("\npassword: ", 11);
    if (read_line(pass, sizeof pass, 0) < 0) return;
    console_app_write("\n", 1);
    i32 r = acc_login(login, pass);
    char msg[64];
    usize o = 0;
    const char *m1 = r == ENCH_EOK ? "vhod ok: " : "oshibka vhoda ";
    const char *m2 = r == ENCH_EOK ? login : "(nevernyj login/parol)";
    for (usize j = 0; m1[j]; j++) msg[o++] = m1[j];
    for (usize j = 0; m2[j] && o < 60; j++) msg[o++] = m2[j];
    msg[o++] = '\n';
    console_app_write(msg, o);
}

static void do_create(void) {
    char login[32], pass[40];
    console_app_write("novyj login: ", 13);
    if (read_line(login, sizeof login, 1) < 0) return;
    console_app_write("\npassword: ", 11);
    if (read_line(pass, sizeof pass, 0) < 0) return;
    console_app_write("\n", 1);
    i32 r = acc_create(login, pass);
    console_app_write(r == ENCH_EOK ? "akkaunt sozdan\n"
                                    : "oshibka sozdaniya (est ili plohoi login)\n", 60);
}

void main(const char *args) {
    (void)args;
    gfx_init();
    gui_desktop();
    i32 w = gui_win_create("accmgr", 4, 8, 220, 96, MYOS_BG);
    gui_win_focus(w);
    draw();
    for (;;) {
        i32 k = gui_poll_key();
        if (k < 0) { task_yield(); continue; }
        if (k == 'q' || k == 27) break;
        if (k == 'r') { draw(); continue; }
        if (k == '1') { do_login(); draw(); }
        if (k == '2') { do_create(); draw(); }
    }
    gui_win_close(w);
}
