// kernel/src/initd/initd.c — init.d_: исполняет *.einit-скрипты.
// Каталог ENCH_INITD_DIR, вход main.einit. Команды строк:
//   load <путь.b_d>            — загрузка драйвера (drv_load)
//   start <путь> [аргумент...] — spawn приложения (ring3 ET_EXEC или
//                                ring0 ET_REL Platform/Apps|MyOS)
//   include <имя.einit>        — вложенный скрипт из того же каталога
//   echo <текст>               — в консоль (kprintf, уважает J4)
//   sleep <мс>                 — пауза boot-контекста (hlt)
//   # ...                      — комментарий
#include <ench/kernel.h>
#include <ench/osstrace.h>

#define LINE_MAX 160

static void run_file(const char *dir, const char *name);

// ---- мини-парсер ----
static char *skip_ws(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// первый токен: пишет в tok (cap), сдвигает *ps на остаток
static void token(char **ps, char *tok, usize cap) {
    char *s = skip_ws(*ps);
    usize i = 0;
    for (; s[i] && s[i] != ' ' && s[i] != '\t' && s[i] != '\n'; i++)
        if (i + 1 < cap) tok[i] = s[i];
    tok[i < cap ? i : cap - 1] = 0;
    *ps = s + i;
}

static u32 parse_u32(const char *s) {
    u32 v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (u32)(*s++ - '0');
    return v;
}

static void run_line(char *line) {
    line = skip_ws(line);
    if (!*line || *line == '#') return;

    char cmd[12], rest[LINE_MAX];
    char *p = line;
    token(&p, cmd, sizeof cmd);
    char *r = skip_ws(p);
    usize rl = 0;
    for (; r[rl] && r[rl] != '\n'; rl++)
        if (rl + 1 < sizeof rest) rest[rl] = r[rl];
    rest[rl] = 0;

    if (ench_strcmp(cmd, "load") == 0) {
        if (!rest[0]) { kprintf("initd: load: нет пути\n"); return; }
        i32 e = drv_load(rest);
        if (e != ENCH_EOK) kprintf("initd: load %s: err=%d\n", rest, e);
    } else if (ench_strcmp(cmd, "start") == 0) {
        char path[96];
        char *q = rest;
        token(&q, path, sizeof path);
        if (!path[0]) { kprintf("initd: start: нет пути\n"); return; }
        // остальные токены — argv[1..]
        const char *av[8];
        char abuf[6][48];
        int ac = 0;
        av[ac++] = path;
        while (ac < 6) {
            char t[48];
            token(&q, t, sizeof t);
            if (!t[0]) break;
            usize tl = ench_strlen(t);
            for (usize i = 0; i <= tl && i < sizeof abuf[0]; i++) abuf[ac][i] = t[i];
            av[ac] = abuf[ac];
            ac++;
        }
        av[ac] = 0;
        i32 pid = 0;
        i32 e = task_spawn_elf(path, av, &pid);
        if (e != ENCH_EOK) kprintf("initd: start %s: err=%d\n", path, e);
        else OssTrace_Trace("initd: start %s pid=%d", path, pid);
    } else if (ench_strcmp(cmd, "include") == 0) {
        if (!rest[0]) { kprintf("initd: include: нет имени\n"); return; }
        run_file(ENCH_INITD_DIR, rest);
    } else if (ench_strcmp(cmd, "echo") == 0) {
        kprintf("%s\n", rest);
    } else if (ench_strcmp(cmd, "sleep") == 0) {
        u64 end = uptime_ms() + parse_u32(rest);
        while (uptime_ms() < end) __asm__ volatile("hlt");
    } else {
        kprintf("initd: неизвестная команда '%s'\n", cmd);
    }
}

static void run_file(const char *dir, const char *name) {
    char path[128];
    fs_path_join(path, sizeof path, dir, name);
    i32 fd = fs_open(path, ENCH_O_READ);
    if (fd < 0) {
        kprintf("initd: %s: err=%d\n", name, fd);
        return;
    }
    static char buf[2048];
    i64 n = fs_read(fd, buf, sizeof buf - 1);
    fs_close(fd);
    if (n <= 0) {
        kprintf("initd: %s: пусто\n", name);
        return;
    }
    buf[n] = 0;
    char *p = buf;
    while (p) {
        char *nl = p;
        while (*nl && *nl != '\n') nl++;
        int ended = (*nl == 0);
        *nl = 0;
        run_line(p);
        if (ended) break;
        p = nl + 1;
    }
}

void initd_run(void) {
    OssTrace_Trace("initd: старт");
    run_file(ENCH_INITD_DIR, "main.einit");
}
