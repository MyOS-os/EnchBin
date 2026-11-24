/* include/ench/fs.h — ramfs + правила имён.
 * Реализация: kernel/src/fs.c (ustar из модуля enchdisk.tar + write-оверлей).
 * Правила:
 *   имя с '.'  -> скрытое (не видно без ls --a)
 *   имя с '__' -> lock: чтение -> err, запись/удаление -> err
 * Пути: абсолютные от '/', относительные — от cwd задачи. */
#ifndef ENCH_FS_H
#define ENCH_FS_H
#include <stdint.h>
#include <stddef.h>
#include <ench/abi.h>

/* канонические пути диска */
#define ENCH_PATH_SYSTEM   "/Platform/data/system"
#define ENCH_PATH_INITD    "/Platform/data/system/init.d_"
#define ENCH_PATH_ACCDIR   "/Platform/data/system/.data-hash-user"
#define ENCH_PATH_LOGDIR   "/Platform/data/system/log"
#define ENCH_PATH_ASMDIR   "/Platform/data/system/asm"
#define ENCH_PATH_DRIVERS  "/Platform/ench.bin/system/drivers"
#define ENCH_PATH_USERD    "/Platform/ench.bin/system/drivers/userd"
#define ENCH_PATH_DRIVE    "/Platform/ench.bin/system/drive"
#define ENCH_PATH_APPS     "/Platform/Apps"
#define ENCH_PATH_MYOS     "/Platform/MyOS"
#define ENCH_PATH_USERAPPS "/User/apps"

#define ENCH_PASSFILE ".pass.__"

/* flags для SYS_open/ench_open */
#define ENCH_O_READ  0
#define ENCH_O_WRITE 1
#define ENCH_O_CREATE 2
#define ENCH_O_APPEND 4

/* правила имен (использует и аккаунт-система, и shell) */
int  fs_name_hidden(const char *name);
int  fs_name_locked(const char *name);

/* правила нет: shell/драйверы видят эти хелперы через ksyms */
long fs_path_join(char *out, size_t cap, const char *dir, const char *name);

#endif
