#pragma once
/* include/ench/libench.h — ring3 API (stdlib/c => libench.a).
 * Единственный способ ring3-приложений User/apps общаться с ядром. */
#include <stdint.h>
#include <stddef.h>
#include <ench/abi.h>

/* syscall-транк */
int64_t enc_syscall(int64_t n, int64_t a1, int64_t a2, int64_t a3,
                    int64_t a4, int64_t a5, int64_t a6);

/* процессы */
void    enc_exit(int64_t code) __attribute__((noreturn));
int64_t enc_spawn(const char *path, char *const argv[]);
int64_t enc_exec(const char *path, char *const argv[]);
int64_t enc_wait(int64_t pid);
int64_t enc_getpid(void);
void    enc_yield(void);
void    enc_sleep(uint64_t ms);
int64_t enc_kill(int64_t pid);
int     enc_ps(struct ench_ps *out);

/* планировщик: вес кода и real-time */
int enc_set_weight(int64_t pid, int64_t weight);
int enc_set_rt(int on);

/* консоль/вывод (J4) */
void enc_print(const char *s);
void enc_printf(const char *fmt, ...);
void enc_clear(void);
void enc_claim_console(void);
void enc_release_console(void);
int  enc_read_key(void);      /* блокирующе, -1 при отсутствии драйвера */

/* файлы */
int  enc_open(const char *path, int flags);
int  enc_close(int fd);
int64_t enc_read(int fd, void *buf, uint64_t len);
int64_t enc_write(int fd, const void *buf, uint64_t len);
int  enc_unlink(const char *path);
int  enc_fsinfo(const char *path, struct ench_fsinfo *out);

/* память */
void *enc_mmap(uint64_t len, int exec);
void *enc_brk(int64_t delta);

/* аккаунты */
int enc_acc_create(const char *login, const char *pass);
int enc_acc_login(const char *login, const char *pass);
void enc_acc_logout(void);
int enc_acc_list(struct ench_acc_list *out);
const char *enc_acc_current(void);

/* драйверы */
int enc_drv_list(struct ench_drv_list *out);
int enc_drv_ioctl(const char *name, int64_t op, int64_t a, int64_t b, void *buf, uint64_t len, int64_t *ret);

/* динамический ассемблер: SYS_runasm */
int enc_runasm(const void *code, uint64_t len, int64_t *result);

/* J4: финальная строка кода завершения (вызывается crt0 автоматически) */
void enc_exit_code(int64_t code);

/* строки (stdlib/c) */
size_t enc_strlen(const char *s);
int    enc_strcmp(const char *a, const char *b);
int    enc_strncmp(const char *a, const char *b, size_t n);
char  *enc_strcpy(char *d, const char *s);
char  *enc_strcat(char *d, const char *s);
void  *enc_memcpy(void *d, const void *s, size_t n);
void  *enc_memset(void *d, int c, size_t n);
int    enc_sprintf(char *buf, const char *fmt, ...);
