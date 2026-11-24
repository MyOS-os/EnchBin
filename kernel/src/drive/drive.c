// kernel/src/drive/drive.c — менеджер драйверов: реестр .b_d-драйверов
// (reloc ELF с экспортом "ench_drv_init", регистрируются через drv_register)
// + встроенный сервис "kernel" (reboot/halt/uptime/kill без .b_d).
#include <ench/kernel.h>
#include <ench/elf.h>
#include <ench/osstrace.h>

#define DRV_MAX 16
static struct ench_drv *drvs[DRV_MAX];
static int drv_n;

// ---- встроенный сервис "kernel" (без .b_d) ----
static int kernel_ioctl(u32 cmd, u64 arg) {
    switch (cmd) {
    case ENCH_KCMD_REBOOT:
        power_reboot();
        return ENCH_EOK;
    case ENCH_KCMD_HALT:
        power_halt();
        return ENCH_EOK;
    case ENCH_KCMD_UPTIME:
        if (arg) *(u64 *)arg = uptime_ms();
        return ENCH_EOK;
    case ENCH_KCMD_KILL:
        task_kill((i32)arg);
        return ENCH_EOK;
    default:
        return ENCH_EINVAL;
    }
}
static struct ench_drv kernel_drv = {
    .name = "kernel", .type = "core",
    .init = 0, .write = 0, .read = 0, .ioctl = kernel_ioctl,
};
static u8 builtin_done;

static void ensure_builtin(void) {
    if (builtin_done) return;
    builtin_done = 1;
    drvs[drv_n++] = &kernel_drv;
}

i32 drv_register(struct ench_drv *d) {
    if (!d || !d->name) return ENCH_EINVAL;
    ensure_builtin();
    if (drv_get(d->name)) return ENCH_EEXIST;
    if (drv_n >= DRV_MAX) return ENCH_ENOSPC;
    drvs[drv_n++] = d;
    OssTrace_Trace("drv: '%s' (%s) зарегистрирован", d->name, d->type ? d->type : "?");
    return ENCH_EOK;
}

struct ench_drv *drv_get(const char *name) {
    ensure_builtin();
    if (!name) return 0;
    for (int i = 0; i < drv_n; i++)
        if (ench_strcmp(drvs[i]->name, name) == 0) return drvs[i];
    return 0;
}

i32 drv_list(char *out, usize cap) {
    ensure_builtin();
    if (!out || !cap) return ENCH_EINVAL;
    usize o = 0;
    for (int i = 0; i < drv_n; i++) {
        const char *nm = drvs[i]->name;
        for (usize j = 0; nm[j] && o + 1 < cap; j++) out[o++] = nm[j];
        if (o + 1 < cap) out[o++] = '\n';
    }
    if (o < cap) out[o] = 0;
    return drv_n;
}

static u8 drv_img[1 << 16]; // буфер образа .b_d (64 КиБ)

i32 drv_load(const char *path) {
    if (!path) return ENCH_EINVAL;
    i32 fd = fs_open(path, ENCH_O_READ);
    if (fd < 0) return fd;
    i64 n = fs_read(fd, drv_img, sizeof drv_img);
    fs_close(fd);
    if (n < (i64)sizeof(struct elf64_ehdr)) return ENCH_EINVAL;
    int before = drv_n;
    i32 err;
    void *fn = elf_rel_load(drv_img, (usize)n, "ench_drv_init", &err);
    if (!fn) {
        kprintf("drv: %s: загрузка не удалась (err=%d)\n", path, err);
        return err;
    }
    ((void (*)(void))fn)();
    if (drv_n == before) {
        kprintf("drv: %s: init не вызвал drv_register\n", path);
        return ENCH_EINVAL;
    }
    kprintf("drv: %s загружен\n", path);
    return ENCH_EOK;
}

i32 drv_call(const char *name, u32 cmd, u64 arg) {
    struct ench_drv *d = drv_get(name);
    if (!d) return ENCH_ENOENT;
    if (!d->ioctl) return ENCH_EINVAL;
    return d->ioctl(cmd, arg);
}
