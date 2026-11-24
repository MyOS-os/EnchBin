// include/ench/libench.h — ring3 API EnchBin: umbrella + расширенный stdlib.
// Ядро вызовов — ench/user.h (syscalls 0..26 через syscall-инструкцию);
// здесь — удобства libench (stdlib/c, собирается в build/libench.a):
// строки, числа, strbuf, argparse, пути и sbrk-аллокатор.
#ifndef ENCH_LIBENCH_H
#define ENCH_LIBENCH_H

#include <ench/base.h>
#include <ench/abi.h>
#include <ench/osstrace.h>
#include <ench/user.h>

// ---- строки/память (freestanding, без libc) ----
usize enc_strlen(const char *s);
int   enc_strcmp(const char *a, const char *b);
int   enc_strncmp(const char *a, const char *b, usize n);
char *enc_strcpy(char *d, const char *s);
char *enc_strcat(char *d, const char *s);
char *enc_strchr(const char *s, char c);
void *enc_memcpy(void *d, const void *s, usize n);
void *enc_memset(void *d, int c, usize n);
int   enc_snprintf(char *buf, usize cap, const char *fmt, ...);
int   enc_vsnprintf(char *buf, usize cap, const char *fmt, va_list ap);

// ---- числа ----
int    enc_atoi(const char *s);            // десятичное, с знаком
usize  enc_utoa(u64 v, char *out, int base); // base 2..16, возвращает длину

// ---- strbuf: растущий буфер на enc_alloc ----
struct enc_strbuf { char *s; usize len, cap; };
int   enc_sb_init(struct enc_strbuf *sb, usize cap);
void  enc_sb_free(struct enc_strbuf *sb);
int   enc_sb_putc(struct enc_strbuf *sb, char c);
int   enc_sb_puts(struct enc_strbuf *sb, const char *s);
int   enc_sb_printf(struct enc_strbuf *sb, const char *fmt, ...);
char *enc_sb_take(struct enc_strbuf *sb);   // отдать буфер (не free)

// ---- argparse: строка -> argv (по пробелам/табам, кавычки "...") ----
// модифицирует строку; возвращает число аргументов (>=0), argv дополняется 0
int enc_argparse(char *line, char **argv, int max);

// ---- пути ----
int enc_path_join(char *out, usize cap, const char *dir, const char *name);
int enc_path_basename(const char *path, char *out, usize cap);
int enc_path_dirname(const char *path, char *out, usize cap);

// ---- аллокатор поверх ench_sbrk (bump, без поэлементного free) ----
void *enc_alloc(usize n);
void  enc_free_all(void);                    // сброс бампа (осторожно: указатели протухают)

#endif
