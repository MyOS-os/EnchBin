/* include/ench/console.h — консоль + протокол J4 (документация).
 * Реализация: kernel/src/console/console.c (шрифт 3x5, framebuffer);
 * объявления — в ench/kernel.h (kconsole_init/kprintf/kclear/ksetattr/
 * kscroll/kfont_glyph/console_acquire/release/console_app_write/
 * console_exit_line/kinput_push/kinput_pop/kserial_*).
 *
 * J4: пока owner != NULL (приложение активно), kprintf ядра уходит только
 * на COM1/filelog; приложение пишет через console_app_write (SYS_write);
 * при завершении owner-задачи ядро печатает единственную строку "exit N"
 * (ENCH_EXIT_LINE_FMT). Цвета ENCH_* (атрибуты 0..15) — в kernel.h.
 * CSI-подмножество обслуживает драйвер ansi.b_d: ESC[2J, ESC[0m, ESC[3Nm,
 * ESC[4Nm, ESC[9Nm (см. ench/drv.h). */
#ifndef ENCH_CONSOLE_H
#define ENCH_CONSOLE_H
#include <ench/kernel.h>
#endif
