// kernel/src/console/fmt.c — общий форматер ядра (kvformat) без libc.
// Поддержка: %s %c %d %i %u %x %X %p %lld %llu %zu, %%.
#include <ench/kernel.h>

static usize u2s(u64 v, u8 base, char *b) {
    char t[24]; usize n = 0;
    if (!v) t[n++] = '0';
    while (v) { u8 d = v % base; t[n++] = d < 10 ? '0' + d : 'a' + d - 10; v /= base; }
    for (usize i = 0; i < n; i++) b[i] = t[n - 1 - i];
    b[n] = 0;
    return n;
}

int kvformat(char *out, usize cap, const char *fmt, va_list ap) {
    usize n = 0;
#define PUT(c) do { if (n + 1 < cap) out[n] = (c); n++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { PUT(*fmt); continue; }
        fmt++;
        int l = 0;
        if (*fmt == 'l') { l = 1; fmt++; if (*fmt == 'l') { l = 2; fmt++; } }
        if (*fmt == 'z') { l = 2; fmt++; }
        char b[24]; const char *s = 0; int neg = 0; u64 u;
        switch (*fmt) {
        case 's': s = va_arg(ap, const char *); if (!s) s = "(null)"; break;
        case 'c': b[0] = (char)va_arg(ap, int); b[1] = 0; s = b; break;
        case 'd': case 'i': {
            i64 v = l == 2 ? va_arg(ap, i64) : l == 1 ? va_arg(ap, long) : va_arg(ap, int);
            if (v < 0) { neg = 1; u = (u64)(-v); } else u = (u64)v;
            usize k = u2s(u, 10, b);
            if (neg && k + 1 < 24) { b[k] = b[0]; b[0] = '-'; b[k + 1] = 0; }
            s = b; break;
        }
        case 'u':
            u = l == 2 ? va_arg(ap, u64) : l == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            u2s(u, 10, b); s = b; break;
        case 'x': case 'X':
            u = l == 2 ? va_arg(ap, u64) : l == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            u2s(u, 16, b); s = b; break;
        case 'p': {
            u = va_arg(ap, u64);
            usize k = u2s(u, 16, b + 2);
            b[0] = '0'; b[1] = 'x'; b[2 + k] = 0;
            s = b; break;
        }
        case '%': PUT('%'); break;
        default: break;
        }
        if (s) while (*s) PUT(*s++);
    }
    if (n < cap) out[n] = 0;
    else if (cap) out[cap - 1] = 0;
    return (int)n;
}
