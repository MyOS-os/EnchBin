/* include/ench/drv.h — реестр сервисов драйверов (kernel/src/drv.c).
 * Драйверы (.b_d, включая userspace/driversuser/msdos) регистрируют call-сервисы;
 * shell/приложения дергают их через SYS_drv_call (ring3) или drv_call (ring0). */
#ifndef ENCH_DRV_H
#define ENCH_DRV_H
#include <stdint.h>
#include <stddef.h>

struct ench_dvrops {
    int (*call)(uint32_t cmd, int64_t a, int64_t b, void *buf, uint64_t len);
};

#define ENCH_DRV_MAX 16

long drv_register(const char *name, const struct ench_dvrops *ops);
long drv_unregister(const char *name);
long drv_call(const char *name, uint32_t cmd, int64_t a, int64_t b, void *buf, uint64_t len);
long drv_list(char *buf, size_t cap); /* "name1\nname2\n" -> длина */

/* встроенный сервис "kernel" (без .b_d) */
#define KERNEL_REBOOT  1
#define KERNEL_HALT    2
#define KERNEL_UPTIME  3 /* a -> ms */
#define KERNEL_TASKS   4 /* buf=ench_taskinfo[] b=cap -> count */
#define KERNEL_KILL    5 /* a=tid */
struct ench_taskinfo {
    long     tid;
    long     ring;
    long     weight;
    long     rt;
    long     state;
    char     name[24];
};

#endif
