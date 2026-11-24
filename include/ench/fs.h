/* include/ench/fs.h — документация ramfs и правил имён.
 * Реализация: kernel/src/fs/fs.c (ustar-модуль enchdisk.tar + write-оверлей);
 * объявления функций — в ench/kernel.h (fs_open/read/write/stat/mkdir/
 * unlink/listdir/chdir/getcwd/log_write/raw_read/name_hidden/name_locked/
 * path_join/init). Режимы ENCH_O_* и struct ench_fstat — в ench/abi.h.
 *
 * Правила имён:
 *   имя с '.' в начале  -> скрытое (не видно без ls --a)
 *   имя с суффиксом ".__" -> скрытое И lock: чтение -> ENCH_ELOCKED,
 *                            запись/удаление -> ENCH_EACCES
 * Пути: абсолютные от '/', относительные — от cwd (fs_chdir/fs_getcwd).
 *
 * Дополнительные канонические пути (сверх abi.h): */
#ifndef ENCH_FS_H
#define ENCH_FS_H

#define ENCH_PATH_SYSTEM   "/Platform/data/system"
#define ENCH_PATH_APPS     "/Platform/Apps"
#define ENCH_PATH_MYOS     "/Platform/MyOS"
#define ENCH_PATH_USERAPPS "/User/apps"

#endif
