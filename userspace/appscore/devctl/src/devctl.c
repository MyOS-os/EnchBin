// userspace/appscore/devctl/src/devctl.c — ring0-приложение (ET_REL "main").
// Управление устройствами: реестр драйверов, вызов встроенного сервиса
// "kernel" (uptime), демонстрация выгрузки последней ET_REL-единицы.
#include <ench/kernel.h>

static void out(const char *s) { console_app_write(s, ench_strlen(s)); }
static void outln(const char *s) { out(s); out("\n"); }

void main(const char *args) {
    (void)args;
    outln("--- devctl ---");

    static char dl[256];
    i32 n = drv_list(dl, sizeof dl);
    if (n < 0) outln("reestr nedostupen");
    else {
        out("draivera v reestre (");
        char num[12];
        usize nn = 0;
        u64 u = (u64)n;
        if (!u) num[nn++] = '0';
        while (u) { num[nn++] = (char)('0' + u % 10); u /= 10; }
        for (usize i = 0; i < nn; i++) out(num + nn - 1 - i);
        outln("):");
        usize i = 0;
        while (i < sizeof dl && dl[i]) {
            char one[32];
            usize l = 0;
            while (dl[i] && dl[i] != '\n' && l + 1 < sizeof one) one[l++] = dl[i++];
            if (dl[i] == '\n') i++;
            one[l] = 0;
            if (l) { out("  "); outln(one); }
        }
    }

    // встроенный сервис kernel: uptime через drv_call
    u64 ms = 0;
    i32 r = drv_call("kernel", ENCH_KCMD_UPTIME, (u64)&ms);
    if (r == ENCH_EOK) {
        out("kernel.uptime: ");
        char num[24];
        usize nn = 0;
        u64 u = ms;
        if (!u) num[nn++] = '0';
        while (u) { num[nn++] = (char)('0' + u % 10); u /= 10; }
        for (usize i = 0; i < nn; i++) out(num + nn - 1 - i);
        outln(" ms");
    } else {
        outln("kernel.uptime: oshibka drv_call");
    }

    // выгрузка последней ET_REL-единицы (devctl unload): безопасно только
    // если единица не используется; в v0.1 просто демонстрируем вызов
    if (args && args[0] == 'u') {
        i32 ur = elf_rel_unload_last();
        outln(ur == ENCH_EOK ? "unload-last: ok" : "unload-last: net edinic");
    }
}
