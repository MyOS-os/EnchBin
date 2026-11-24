// kernel/src/task/task.c — планировщик weighted round-robin + rt-потоки.
// Вес по умолчанию = размер ELF-образа/1024 (клампится 1..1024), квант = базовый*вес.
// rt-поток не вытесняется по тику (SYS_setrt) — сам должен заблокироваться/yield.
// ring3-приложения (ET_EXEC, база 0x400000) и ring0-приложения appscore/MyOS
// (ET_REL с экспортом "main", грузятся elf_rel_load) спавнятся через SYS_spawn.
#include <ench/kernel.h>
#include <ench/kcpu.h>
#include <ench/elf.h>
#include <ench/osstrace.h>

#define MAX_TASKS 64
#define SLICE_BASE 10
#define USER_STACK_TOP 0x70000000ull
#define USER_BRK_BASE 0x60000000ull
#define KSTACK_SIZE 8192

struct task {
    i32 pid; char name[16];
    u8 state, rt, user, zombie;
    u32 weight; i32 slice;
    struct cpu_frame *frame;
    u64 kstack, kstack_top;
    i32 parent, exit_code;
    void (*fn)(void *); void *arg;
    u64 brk, wake_at;
};

static struct task tasks[MAX_TASKS];
static struct task *cur;
static i32 next_pid = 1;
static u32 rr;

static struct task *by_pid(i32 pid) {
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].pid == pid && tasks[i].kstack) return &tasks[i];
    return 0;
}

static struct task *alloc_slot(void) {
    for (int i = 0; i < MAX_TASKS; i++)
        if (!tasks[i].kstack) { ench_memset(&tasks[i], 0, sizeof tasks[i]); return &tasks[i]; }
    return 0;
}

static int is_ready(struct task *t) { return t->kstack && t->state == ENCH_TASK_READY; }

static struct task *pick_next_rt(void) {
    for (u32 i = 1; i <= MAX_TASKS; i++) {
        struct task *t = &tasks[(rr + i) % MAX_TASKS];
        if (t != cur && is_ready(t) && t->rt) return t;
    }
    return 0;
}

static struct task *pick_next(void) {
    // 1) готовые rt-потоки — абсолютный приоритет
    struct task *t = pick_next_rt();
    if (t) return t;
    // 2) взвешенный round-robin по остальным
    for (u32 i = 1; i <= MAX_TASKS; i++) {
        struct task *x = &tasks[(rr + i) % MAX_TASKS];
        if (x != cur && is_ready(x)) return x;
    }
    return 0;
}

static void *switch_to(struct task *t) {
    if (cur && cur->state == ENCH_TASK_RUN) cur->state = ENCH_TASK_READY;
    cur = t;
    cur->state = ENCH_TASK_RUN;
    cur->slice = SLICE_BASE * (i32)cur->weight;
    rr = (u32)(t - tasks);
    cpu_set_ksp(cur->kstack_top);
    cpu_set_tss_rsp0(cur->kstack_top);
    return cur->frame;
}

void task_init(void) {
    ench_memset(tasks, 0, sizeof tasks);
    cur = 0; next_pid = 1; rr = 0;
}

// тик PIT: вернуть кадр новой задачи или 0 (продолжить текущую)
void *sched_timer(struct cpu_frame *fr) {
    if (cur) {
        cur->frame = fr;
        if (cur->rt) return 0;             // rt не вытесняется по тику
        if (cur->slice > 0 && !pick_next_rt()) { cur->slice--; return 0; }
    }
    // разбудить заснувшие по времени
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].kstack && tasks[i].state == ENCH_TASK_SLEEP && uptime_ms() >= tasks[i].wake_at)
            tasks[i].state = ENCH_TASK_READY;
    struct task *n = pick_next();
    if (!n) return 0;
    return switch_to(n);
}

// ---- резервный кадр idle: работает, когда задач нет ----
static u64 dead_kstack[512];
static void idle_loop(void) { for (;;) __asm__ volatile("sti; hlt"); }
static void *dead_frame(void) {
    static struct cpu_frame *df;
    if (!df) df = cpu_make_frame((u64)dead_kstack + sizeof dead_kstack - 16, (u64)idle_loop, 0, 0);
    cur = 0;
    cpu_set_ksp((u64)dead_kstack + sizeof dead_kstack - 16);
    return df;
}

// для ручного тика из не-IRQ контекста (тесты/внешние источники)
void scheduler_tick(void) { }

u32 task_weight_for_size(usize n) {
    u32 w = (u32)(n / 1024);
    if (w < 1) w = 1;
    if (w > 1024) w = 1024;
    return w;
}

// ---- общий хвост спавна ----
static struct task *spawn_common(const char *name, i32 weight) {
    struct task *t = alloc_slot();
    if (!t) return 0;
    t->pid = next_pid++;
    for (int i = 0; i < 15 && name[i]; i++) t->name[i] = name[i];
    t->parent = cur ? cur->pid : 0;
    t->weight = (u32)weight;
    t->brk = USER_BRK_BASE;
    t->kstack = (u64)kmalloc(KSTACK_SIZE);
    if (!t->kstack) return 0;
    t->kstack_top = t->kstack + KSTACK_SIZE - 16;
    return t;
}

static void name_of(const char *path, char out[16]) {
    const char *nm = path;
    for (const char *p = path; *p; p++) if (*p == '/') nm = p + 1;
    for (int i = 0; i < 15 && nm[i]; i++) out[i] = nm[i];
    out[15] = 0;
}

static u8 imgbuf[1 << 20]; // буфер образа приложения (1 МиБ)

static void kth_app_bootstrap(void);

// ---- спавн приложения: ET_EXEC -> ring3, ET_REL -> ring0 (appscore/MyOS) ----
i32 task_spawn_elf(const char *path, const char *const *argv, i32 *pid_out) {
    i32 fd = fs_open(path, ENCH_O_READ);
    if (fd < 0) return fd;
    struct ench_fstat st;
    if (fs_stat(path, &st) != ENCH_EOK) { fs_close(fd); return ENCH_ENOENT; }
    if (st.size > (i64)sizeof imgbuf) { fs_close(fd); return ENCH_ENOMEM; }
    i64 rd = fs_read(fd, imgbuf, (usize)st.size);
    fs_close(fd);
    if (rd < (i64)sizeof(struct elf64_ehdr)) return ENCH_EINVAL;

    struct elf64_ehdr *eh = (struct elf64_ehdr *)imgbuf;
    char nm[16]; name_of(path, nm);

    if (eh->e_type == ET_REL) {
        // ring0-приложение (Platform/Apps, Platform/MyOS): экспорт "main"
        i32 err;
        void *fn = elf_rel_load(imgbuf, (usize)rd, "main", &err);
        if (!fn) return ENCH_EINVAL;
        struct task *t = spawn_common(nm, 1);
        if (!t) return ENCH_ENOMEM;
        t->user = 1; t->rt = 0;
        t->fn = fn; t->arg = 0;
        t->frame = cpu_make_frame(t->kstack_top, (u64)kth_app_bootstrap, 0, 0);
        t->state = ENCH_TASK_READY;
        console_acquire(t);                 // J4
        if (pid_out) *pid_out = t->pid;
        OssTrace_Trace("spawn0 %s pid=%d", t->name, t->pid);
        return ENCH_EOK;
    }

    if (eh->e_type != ET_EXEC) return ENCH_EINVAL;
    u64 entry, stack_top;
    if (elf_load_ring3(imgbuf, (usize)rd, &entry, &stack_top) != ENCH_EOK) return ENCH_EINVAL;

    struct task *t = spawn_common(nm, (i32)task_weight_for_size((usize)rd));
    if (!t) return ENCH_ENOMEM;
    t->user = 1;

    // argc/argv на пользовательском стеке (SysV: argc по rsp, argv выше)
    u64 sp = USER_STACK_TOP;
    const char *av[8]; int ac = 0;
    if (argv) while (ac < 8 && argv[ac]) { av[ac] = argv[ac]; ac++; }
    for (int i = ac - 1; i >= 0; i--) {
        usize l = ench_strlen(av[i]) + 1;
        sp -= l;
        ench_memcpy((void *)sp, av[i], l);
        av[i] = (const char *)sp;
    }
    sp &= ~15ull;
    sp -= 8 * (ac + 1);
    u64 *vec = (u64 *)sp;
    vec[0] = (u64)ac;
    for (int i = 0; i < ac; i++) vec[1 + i] = (u64)av[i];

    t->frame = cpu_make_frame(t->kstack_top, entry, sp, 1);
    t->state = ENCH_TASK_READY;
    console_acquire(t);                    // J4: приложение получает экран
    if (pid_out) *pid_out = t->pid;
    OssTrace_Trace("spawn %s pid=%d weight=%d", t->name, t->pid, t->weight);
    return ENCH_EOK;
}

// ---- kernel-поток (initd, демоны) и вход ring0-приложения ----
static void kth_bootstrap(void) {
    cur->fn(cur->arg);
    task_exit(0);
}

static void kth_app_bootstrap(void) {
    ((void (*)(const char *))cur->fn)((const char *)cur->arg);
    task_exit(0);
}

i32 task_spawn_kth(void (*fn)(void *), void *arg, const char *name) {
    struct task *t = spawn_common(name, 1);
    if (!t) return ENCH_ENOMEM;
    t->fn = fn; t->arg = arg;
    t->frame = cpu_make_frame(t->kstack_top, (u64)kth_bootstrap, 0, 0);
    t->state = ENCH_TASK_READY;
    return t->pid;
}

// ---- завершение ----
void task_exit_core(i32 code) {
    if (!cur) return;
    cur->state = ENCH_TASK_DEAD;
    cur->exit_code = code;
    cur->zombie = 1;
    if (cur->user) console_exit_line(code); // J4: последняя строка "exit N"
    console_release(cur);
    for (int i = 0; i < MAX_TASKS; i++)     // разбудить ждущего родителя
        if (tasks[i].kstack && tasks[i].state == ENCH_TASK_BLOCK && tasks[i].parent == cur->pid)
            tasks[i].state = ENCH_TASK_READY;
    OssTrace_Trace("exit pid=%d code=%d", cur->pid, code);
}

void task_exit(i32 code) {
    task_exit_core(code);
    cpu_restore_frame(task_exit_switch()); // переключиться на следующую задачу
}

// для SYS_exit: sys_dispatch сам вернёт кадр следующей задачи
void *task_exit_switch(void) {
    struct task *n = pick_next();
    if (n) return switch_to(n);
    return dead_frame();
}

void *task_yield_switch(void *fr) {
    if (!cur) return fr;
    cur->frame = fr;
    struct task *n = pick_next();
    if (!n) return fr;
    return switch_to(n);
}

// ---- сон (SYS_sleep): цикл в syscall.c через task_yield_switch ----
void task_sleep_begin(u64 ms) {
    if (!cur) return;
    cur->wake_at = uptime_ms() + ms;
    cur->state = ENCH_TASK_SLEEP;
}
int task_sleep_pending(void) {
    return cur && cur->state == ENCH_TASK_SLEEP && uptime_ms() < cur->wake_at;
}
void task_sleep_end(void) {
    if (cur && cur->state == ENCH_TASK_SLEEP) cur->state = ENCH_TASK_RUN;
}

// ---- ожидание/reap ----
void task_reap(i32 pid) {
    struct task *t = by_pid(pid);
    if (!t || !t->zombie) return;
    kfree((void *)t->kstack);
    t->kstack = 0;
}

void task_setweight(i32 pid, u32 w) {
    struct task *t = by_pid(pid);
    if (!t) return;
    if (w < 1) w = 1;
    if (w > 1024) w = 1024;
    t->weight = w;
}

void task_setrt(i32 pid, int on) {
    struct task *t = by_pid(pid);
    if (t) t->rt = on ? 1 : 0;
}

void task_kill(i32 pid) {
    struct task *t = by_pid(pid);
    if (!t || t == cur) return;
    t->state = ENCH_TASK_DEAD;
    t->exit_code = -1;
    t->zombie = 1;
    if (t->user) console_exit_line(-1);
    console_release(t);
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].kstack && tasks[i].state == ENCH_TASK_BLOCK && tasks[i].parent == pid)
            tasks[i].state = ENCH_TASK_READY;
}

i32 task_list(struct ench_taskinfo *out, i32 max) {
    i32 n = 0;
    for (int i = 0; i < MAX_TASKS && n < max; i++) {
        if (!tasks[i].kstack) continue;
        out[n].pid = tasks[i].pid;
        for (int j = 0; j < 15 && tasks[i].name[j]; j++) out[n].name[j] = tasks[i].name[j];
        out[n].name[15] = 0;
        out[n].state = tasks[i].state;
        out[n].rt = tasks[i].rt;
        out[n].weight = tasks[i].weight;
        n++;
    }
    return n;
}

void task_yield(void) { if (cur) cur->slice = 0; }
void task_enter_idle(void) { for (;;) __asm__ volatile("sti; hlt"); }

// ---- brk (SYS_sbrk) ----
u64 task_getcur_brk(void) { return cur ? cur->brk : USER_BRK_BASE; }
void task_getcur_brk_set(u64 nb) { if (cur) cur->brk = nb; }
void *task_cur_ptr(void) { return cur; }

// ---- хелперы sys_dispatch: wait/block/run/pid ----
i32 task_current_pid(void) { return cur ? cur->pid : 0; }
void task_set_block(void) { if (cur) cur->state = ENCH_TASK_BLOCK; }
void task_set_run(void) { if (cur && cur->state == ENCH_TASK_BLOCK) cur->state = ENCH_TASK_RUN; }

// 1 = ребёнок DEAD (код в *code_out), 0 = ещё работает, <0 = errno.
// pid < 0 => любой zombie-ребёнок (возвращает его pid).
i32 task_wait_check(i32 pid, i32 *code_out) {
    if (!cur) return ENCH_EINVAL;
    if (pid < 0) {
        for (int i = 0; i < MAX_TASKS; i++)
            if (tasks[i].kstack && tasks[i].zombie && tasks[i].parent == cur->pid) {
                if (code_out) *code_out = tasks[i].exit_code;
                return tasks[i].pid;
            }
        return 0;
    }
    struct task *t = by_pid(pid);
    if (!t || t->parent != cur->pid) return ENCH_EINVAL;
    if (t->state == ENCH_TASK_DEAD) {
        if (code_out) *code_out = t->exit_code;
        return pid;
    }
    return 0;
}
