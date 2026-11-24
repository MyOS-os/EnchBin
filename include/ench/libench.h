/* include/ench/libench.h — ring3 API (stdlib/c -> libench.a).
 * Единственный способ User/apps общаться с ядром; без libc, freestanding. */
#ifndef ENCH_LIBENCH_H
#define ENCH_LIBENCH_H
#include <stdint.h>
#include <stddef.h>
#include <ench/abi.h>
#include <ench/fs.h>

/* консоль */
long ench_print(const char *s);
long ench_printf(const char *fmt, ...);
void ench_clear(void);
int  ench_readc(void); /* блокирует до клавиши; -1 при ошибке */

/* файлы (fd) */
int  ench_open(const char *path, int flags);
int  ench_close(int fd);
long ench_read(int fd, void *buf, size_t len);
long ench_write(int fd, const void *buf, size_t len);
long ench_seek(int fd, long off, int whence);
long ench_stat(int fd, struct ench_fs_ent *out);
long ench_list(const char *dirpath, struct ench_fs_ent *out, size_t max);
long ench_mkdir(const char *dirpath, const char *name);
long ench_create(const char *dirpath, const char *name);
long ench_remove(const char *path);

/* процессы */
long ench_spawn(const char *path, const char *args);
long ench_wait(long tid);
void ench_exit(long code) __attribute__((noreturn));
long ench_getpid(void);
void ench_sleep(long ms);
long ench_gettime(void);
void ench_yield(void);
const char *ench_argstr(void);

/* планировщик */
long ench_setweight(long tid, long w);
long ench_setrt(long tid, int on);

/* модули/драйверы */
long ench_drive(long cmd, long a, long b, void *buf, long len);
long ench_drv(const char *name, long cmd, long a, long b, void *buf, long len);

/* GUI */
long ench_gui_open(int x, int y, int w, int h, const char *title);
long ench_gui_draw(long id, long cmd, long a, long b, long c, long d);
long ench_gui_poll(long id);

/* аккаунты (через fs; хеш FNV-1a64+соль, как в ядре) */
long ench_account_create(const char *login, const char *pass);
long ench_account_verify(const char *login, const char *pass);
long ench_account_count(void);

/* строки (внутренние, без libc) */
size_t enc_strlen(const char *s);
int    enc_strcmp(const char *a, const char *b);
int    enc_strncmp(const char *a, const char *b, size_t n);
char  *enc_strcpy(char *d, const char *s);
char  *enc_strcat(char *d, const char *s);
void  *enc_memcpy(void *d, const void *s, size_t n);
void  *enc_memset(void *d, int c, size_t n);
long   enc_snprintf(char *buf, size_t cap, const char *fmt, ...);
long   enc_vsnprintf(char *buf, size_t cap, const char *fmt, void *va);

#endif
