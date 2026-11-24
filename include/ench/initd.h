#pragma once
/* include/ench/initd.h — init.d_ скрипты (.einit) — документация.
 * Реализация: kernel/src/initd/initd.c; объявление — initd_run() в kernel.h.
 * Каталог ENCH_INITD_DIR ("/Platform/data/system/init.d_"), вход main.einit.
 * Синтаксис строк:
 *   load <путь.b_d>            — загрузка драйвера (drv_load)
 *   start <путь> [аргумент...] — spawn приложения (ET_EXEC ring3 или
 *                                ET_REL ring0: Platform/Apps|MyOS)
 *   include <имя.einit>        — вложенный скрипт из того же каталога
 *   echo <текст>               — в консоль (kprintf, уважает J4)
 *   sleep <мс>                 — пауза boot-контекста
 *   # ...                      — комментарий */
