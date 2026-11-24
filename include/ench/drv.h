/* include/ench/drv.h — реестр драйверов .b_d (документация).
 * Реализация: kernel/src/drive/drive.c; struct ench_drv, ENCH_DRVCTL_* и
 * ENCH_KCMD_* — в ench/abi.h; объявления drv_* — в ench/kernel.h.
 *
 * Конвенция .b_d (reloc ELF, ld -r): обязан экспортировать
 *   int ench_drv_init(void);
 * внутри — cpu_set_kbd_hook/kprintf/console_app_write/drv_register/fs_* и
 * прочее ядро через таблицу ksyms; init обязан вызвать drv_register().
 *
 * Встроенный сервис "kernel" (без .b_d): cmd ENCH_KCMD_REBOOT/HALT/UPTIME.
 *
 * ABI ioctl-команд драйверов (ring3 вызывает через SYS_drvctl CALL):
 *   keyboard: cmd 1 — сброс shift/caps
 *   ansi:     cmd 1 — очистка экрана; cmd 2 — setattr (arg = fg | bg<<8)
 *   unix_lin: cmd 1 — LINE_READ, arg = &struct ench_line_req {
 *                char *buf; usize cap; } (блокирующе до '\n', echo+backspace),
 *             возвращает длину или err; cmd 2 — echo вкл/выкл (arg=0/1)
 *   msdos:    cmd 1 — DIR83, arg = &struct ench_dir83_req {
 *                const char *path; char *out; usize cap; } (листинг 8.3) */
#ifndef ENCH_DRV_H
#define ENCH_DRV_H
#include <ench/abi.h>
#endif
