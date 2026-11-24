#pragma once
/* include/ench/drive.h — drive-подсистема: реестр .mke-модулей и .b_d-драйверов.
 * drive.mke встроен в ядро секцией .enchmke (builder: embed) и активируется
 * при старте. init.d_/drivers.einit грузит .b_d в драйверную арену памяти
 * (drv_arena), инъекция списка — через initd. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ENCH_MKE_MAX 16
#define ENCH_BD_MAX  32

struct drive_unit {
    char     name[24];
    void    *init_fn;       /* int init(void) */
    void    *desc;          /* struct drv_desc* для .b_d */
    int      loaded;
    size_t   size;
    bool     embedded;      /* .mke из секции .enchmke */
};

void drive_init(void);                    /* скан .enchmke + загрузка drive.mke */
int  drive_load_embedded(void);           /* activate drive.mke из секции ядра */
int  drive_load_b_d(const char *abs_path);/* .b_d из ramfs -> арена -> init */
struct drive_unit *drive_find(const char *name);
int  drive_list(struct ench_drv_list *out);
int  drive_ioctl(const char *name, uint64_t op, int64_t a, int64_t b, void *buf, uint64_t len, int64_t *ret);
int  drive_unload_last(void);

/* диспетчер SYS_drvctl */
int64_t drive_syscall(struct ench_drvctl *dc);
