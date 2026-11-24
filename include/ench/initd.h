#pragma once
/* include/ench/initd.h — init.d_ скрипты (.einit).
 * Каталог /Platform/data/system/init.d_:
 *   drivers.einit   — загрузка .b_d-драйверов
 *   autostart.einit — include drivers.einit + запуск shell
 * Синтаксис строк:
 *   load  <путь к .b_d>         — загрузка драйвера в drive-арену
 *   start <путь> [аргументы...] — spawn приложения (ring0/3 по расположению)
 *   include <имя.einit>         — вложенный скрипт из того же каталога
 *   echo <текст>                — в консоль (уважает J4)
 *   # ...                       — комментарий
 */
#include <stdint.h>

int initd_run_dir(const char *dir);
int initd_run_file(const char *abs_path);
