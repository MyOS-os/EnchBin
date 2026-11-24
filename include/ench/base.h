// include/ench/base.h — базовые типы, коды ошибок и мини-утилиты EnchBin OS.
#pragma once

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;
typedef u64 usize;
typedef i64 isize;
typedef u64 uptr;

// Коды ошибок: syscall возвращает отрицательное значение при ошибке.
#define ENCH_EOK       0
#define ENCH_EPERM    -1
#define ENCH_ENOENT   -2
#define ENCH_EIO      -3
#define ENCH_EBADF    -4
#define ENCH_ENOMEM   -5
#define ENCH_EACCES   -6
#define ENCH_EEXIST   -7
#define ENCH_EINVAL   -8
#define ENCH_ENOSPC   -9
#define ENCH_ENOTDIR  -10
#define ENCH_EISDIR   -11
#define ENCH_ELOCKED  -12 /* файл под lock (суффикс .__): cat .pass.__ => err */
#define ENCH_EUNKNOWN -13

#define ENCH_MIN(a, b)      ((a) < (b) ? (a) : (b))
#define ENCH_MAX(a, b)      ((a) > (b) ? (a) : (b))
#define ENCH_ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define ENCH_ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

static inline void ench_memset(void *d, u8 v, usize n) {
    u8 *p = d;
    while (n--) *p++ = v;
}
static inline void ench_memcpy(void *d, const void *s, usize n) {
    u8 *pd = d; const u8 *ps = s;
    while (n--) *pd++ = *ps++;
}
static inline usize ench_strlen(const char *s) {
    usize n = 0;
    while (s && s[n]) n++;
    return n;
}
static inline int ench_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}
static inline int ench_strncmp(const char *a, const char *b, usize n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    return n ? (u8)*a - (u8)*b : 0;
}
