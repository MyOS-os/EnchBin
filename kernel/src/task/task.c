// kernel/src/task/task.c — планировщик weighted round-robin + rt-потоки.
// Вес по умолчанию = размер ELF-образа/1024 (клампится 1..1024), квант = базовый*вес.
// rt-поток не вытесняется по тику (SYS_setrt) — сам должен заблокироваться/yield.
#include <ench/kernel.h>

#define MAX_TASKS 64
#define SLICE_BASE 10
#define USER_IMG_BASE 0x400000ull
#define USER_STACK_TOP 0x70000000ull
#define USER_BRK_BASE 0x60000000ull

struct task {
    i32 pid; char name[16];
    u8 state, rt;
    u32 weight; i32 slice;
    struct cpu_frame *frame;
    u64 kstack, kstack_top;
    i32 parent, exit_code, zombie;
    void (*fn)(void *); void *arg;
    u64 brk;
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

extern void *cpu_make_frame(u64 kstack_top, u64 rip, u64 rsp, u8 user);
extern void *sched_timer(struct cpu_frame *fr); // вызывается из cpu_isr

static struct task *alloc_slot(void) {
    for (int i = 0; i < MAX_TASKS; i++)
        if (!tasks[i].kstack) { ench_memset(&tasks[i], 0, sizeof tasks[i]); return &tasks[i]; }
    return 0;
}

static struct task *pick_next(void) {
    // 1) готовые rt-потоки — абсолютный приоритет
    for (u32 i = 1; i <= MAX_TASKS; i++) {
        struct task *t = &tasks[(rr + i) % MAX_TASKS];
        if (t->kstack && t != cur && (t->state == ENCH_TASK_READY) && t->rt)
            return t;
    }
    // 2) взвешенный RR по остальным
    for (u32 i = 1; i <= MAX_TASKS; i++) {
        struct task *t = &tasks[(rr + i) % MAX_TASKS];
        if (t->kstack && t != cur && t->state == ENCH_TASK_READY) return t;
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

// таймер: вызывается из cpu_isr; возвращает новый кадр или 0
void *sched_timer(struct cpu_frame *fr) {
    if (cur) {
        cur->frame = fr;
        if (cur->rt) return 0;             // rt не вытесняется по тику
        if (cur->slice > 0 && !pick_next_rt_only()) { cur->slice--; return 0; }
    }
    struct task *n = pick_next();
    if (!n) {
        if (cur && cur->state != ENCH_TASK_DEAD) return 0;
        return dead_frame();               // все мертвы — кадр null-задачи
    }
    return switch_to(n);
}

static struct task *pick_next_rt_only(void) {
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].kstack && tasks[i] != cur && tasks[i].state == ENCH_TASK_READY && tasks[i].rt)
            return &tasks[i];
    return 0;
}

static void *dead_frame(void);
static u64 dead_kstack[64];
static void idle_loop(void) { for (;;) __asm__ volatile("sti; hlt"); }
static void *dead_frame(void) {
    static struct cpu_frame *df;
    if (!df) df = cpu_make_frame((u64)dead_kstack + sizeof dead_kstack - 16, (u64)idle_loop, 0, 0);
    cur = 0;
    cpu_set_ksp((u64)dead_kstack + sizeof dead_kstack - 16);
    return df;
}

u32 task_weight_for_size(usize n) {
    u32 w = (u32)(n / 1024);
    if (w < 1) w = 1;
    if (w > 1024) w = 1024;
    return w;
}

// ---- спавн ring3 ELF ----
i32 task_spawn_elf(const char *path, const char *const *argv, i32 *pid_out) {
    i32 fd = fs_open(path, ENCH_O_READ);
    if (fd < 0) return fd;
    struct ench_fstat st;
    fs_stat(path, &st);
    static u8 imgbuf[1 << 20];
    if (st.size > sizeof imgbuf) { fs_close(fd); return ENCH_ENOMEM; }
    i64 rd = fs_read(fd, imgbuf, st.size);
    fs_close(fd);
    if (rd < 0) return ENCH_EIO;

    u64 entry, stack_top;
    if (elf_load_ring3(imgbuf, (usize)rd, &entry, &stack_top) != ENCH_EOK) return ENCH_EINVAL;

    struct task *t = alloc_slot();
    if (!t) return ENCH_ENOMEM;
    t->pid = next_pid++;
    const char *nm = path;
    for (const char *p = path; *p; p++) if (*p == '/') nm = p + 1;
    for (int i = 0; i < 15 && nm[i]; i++) t->name[i] = nm[i];
    t->parent = cur ? cur->pid : 0;
    t->weight = task_weight_for_size((usize)rd);
    t->brk = USER_BRK_BASE;
    t->kstack = (u64)kmalloc(8192);
    if (!t->kstack) return ENCH_ENOMEM;
    t->kstack_top = t->kstack + 8192 - 16;

    // argc/argv на пользовательском стеке
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

// ---- kernel-поток (initd, демоны) ----
static void kth_bootstrap(void) {
    struct task *t = cur;
    t->fn(t->arg);
    task_exit(0);
}

i32 task_spawn_kth(void (*fn)(void *), void *arg, const char *name) {
    struct task *t = alloc_slot();
    if (!t) return ENCH_ENOMEM;
    t->pid = next_pid++;
    for (int i = 0; i < 15 && name[i]; i++) t->name[i] = name[i];
    t->weight = 1;
    t->fn = fn; t->arg = arg;
    t->kstack = (u64)kmalloc(8192);
    if (!t->kstack) return ENCH_ENOMEM;
    t->kstack_top = t->kstack + 8192 - 16;
    t->frame = cpu_make_frame(t->kstack_top, (u64)kth_bootstrap, t->kstack_top - 512, 0);
    t->state = ENCH_TASK_READY;
    return t->pid;
}

void task_exit(i32 code) {
    if (!cur) return;
    cur->state = ENCH_TASK_DEAD;
    cur->exit_code = code;
    cur->zombie = 1;
    console_exit_line(code);               // J4: последняя строка
    console_release(cur);
    for (int i = 0; i < MAX_TASKS; i++)    // разбудить ждущего родителя
        if (tasks[i].kstack && tasks[i].state == ENCH_TASK_BLOCK && tasks[i].parent == cur->pid)
            tasks[i].state = ENCH_TASK_READY;
    OssTrace_Trace("exit pid=%d code=%d", cur->pid, code);
}

void task_yield(void) { cur->slice = 0; }  // обрабатывается в sys_dispatch

// вызывается из SYS_exit после task_exit: вернёт кадр следующей задачи
void *task_exit_switch(void) {
    struct task *n = pick_next();
    if (n) return switch_to(n);
    return dead_frame();
}

i32 task_wait(i32 pid, i32 *code_out) {
    struct task *t = by_pid(pid);
    if (!t || (cur && t->parent != cur->pid)) return ENCH_EINVAL;
    while (t->state != ENCH_TASK_DEAD) {
        cur->state = ENCH_TASK_BLOCK;
        cur->frame = 0;
        __asm__ volatile("sti; hlt");
    }
    if (code_out) *code_out = t->exit_code;
    t->zombie = 0;
    return ENCH_EOK;
}

void task_setweight(i32 pid, u32 w) {
    struct task *t = by_pid(pid);
    if (!t) return;
    if (w < 1) w = 1; if (w > 1024) w = 1024;
    t->weight = w;
}

void task_setrt(i32 pid, int on) {
    struct task *t = by_pid(pid);
    if (t) t->rt = on ? 1 : 0;
}

void task_kill(i32 pid) {
    struct task *t = by_pid(pid);
    if (!t || t == cur) return;
    t->state = ENCH_TASK_DEAD; t->exit_code = -1; t->zombie = 1;
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

void task_enter_idle(void) { sti; }
void task_getcur_brk_set(u64 nb) { if (cur) cur->brk = nb; }
u64 task_getcur_brk(void) { return cur ? cur->brk : USER_BRK_BASE; }
void *task_cur_ptr(void) { return cur; }
