// userspace/appscore/sysinfo/src/sysinfo.c — ring0-приложение (ET_REL "main").
// Сводка системы: версия, uptime, память, задачи, драйверы, аккаунты.
// Вывод через console_app_write (J4: мы владеем экраном).
#include <ench/kernel.h>
#include <ench/task.h>

static void line(const char *a, const char *b) {
    char buf[160];
    usize o = 0;
    for (usize i = 0; a[i] && o < 120; i++) buf[o++] = a[i];
    for (usize i = 0; b[i] && o < 150; i++) buf[o++] = b[i];
    buf[o++] = '\n';
    console_app_write(buf, o);
}

static void line_u(const char *a, u64 v, const char *suffix) {
    char num[24];
    usize n = 0;
    u64 u = v;
    if (!u) num[n++] = '0';
    while (u) { num[n++] = (char)('0' + u % 10); u /= 10; }
    char out[40];
    for (usize i = 0; i < n; i++) out[i] = num[n - 1 - i];
    out[n] = 0;
    line(a, out);
    (void)suffix;
}

void main(const char *args) {
    (void)args;
    line("EnchBin: ", ench_version);
    line_u("uptime (ms): ", uptime_ms(), "");
    line_u("pamyat vsego (KB): ", pmm_total_kb(), "");
    line_u("pamyat svobodno (KB): ", pmm_free_kb(), "");

    static struct ench_taskinfo ti[ENCH_TASK_MAX];
    i32 n = task_list(ti, ENCH_TASK_MAX);
    line_u("zadach: ", (u64)(n < 0 ? 0 : n), "");

    static char dl[256];
    i32 dn = drv_list(dl, sizeof dl);
    line_u("draiverov: ", (u64)(dn < 0 ? 0 : dn), "");
    for (usize i = 0; i < sizeof dl && dl[i];) {
        char one[32];
        usize l = 0;
        while (dl[i] && dl[i] != '\n' && l + 1 < sizeof one) one[l++] = dl[i++];
        if (dl[i] == '\n') i++;
        one[l] = 0;
        if (l) line("  drv: ", one);
    }

    line_u("akkauntov: ", (u64)(acc_count() < 0 ? 0 : acc_count()), "");
    const char *u = acc_current();
    line("polzovatel: ", u ? u : "(ne voshli)");
}
