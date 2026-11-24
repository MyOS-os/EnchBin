// userspace/appscore/myos/src/desktop.c — MyOS рабочий стол (ring 0, ET_REL "main").
// Запуск программ: цифры 1..4 — taskman/fileman/accmgr/sysinfo;
// окно-панель с кнопками (клавиатурный фокус: Tab или стрелки не в v0.1 —
// кнопки кликабельны только визуально, запуск — по цифрам). Выход: q.
#include <ench/kernel.h>
#include <ench/myos.h>
#include <ench/fs.h>

static void launch(const char *path) {
    i32 pid = 0;
    i32 r = task_spawn_elf(path, 0, &pid);
    if (r != ENCH_EOK) {
        // наш вывод всё ещё виден (мы owner до передачи, но spawn уже отдал
        // экран ребёнку) — пишем через serial-консоль ядра
        kprintf("desktop: %s: err=%d\n", path, r);
    }
}

void main(const char *args) {
    (void)args;
    gui_desktop();

    i32 w = gui_win_create("MyOS: programs", 8, 14, 150, 46, MYOS_BG);
    gui_win_focus(w);
    gui_btn_add(w, "1 taskman", 4, 8, 66, 8);
    gui_btn_add(w, "2 fileman", 74, 8, 66, 8);
    gui_btn_add(w, "3 accmgr",  4, 18, 66, 8);
    gui_btn_add(w, "4 sysinfo", 74, 18, 66, 8);
    gui_win_draw(w);
    gfx_text(10, 40, "1..4 - zapusk, q - vyhod", MYOS_TEXT, 1);

    for (;;) {
        i32 k = gui_poll_key();
        if (k < 0) { task_yield(); continue; }
        if (k == 'q' || k == 27) break;
        if (k == '1') launch(ENCH_PATH_MYOS "/taskman");
        if (k == '2') launch(ENCH_PATH_MYOS "/fileman");
        if (k == '3') launch(ENCH_PATH_MYOS "/accmgr");
        if (k == '4') launch(ENCH_PATH_APPS "/sysinfo");
        gui_win_draw(w);
    }
}
