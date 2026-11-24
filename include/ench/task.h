#pragma once
/* include/ench/task.h — задачи и планировщик EnchBin.
 *
 * Взвешенный планировщик: вес задачи = "вес кода" (размер её ELF; меняется
 * SYS_setweight). Планирование round-robin с кредитом: каждой готовой задаче
 * credit += weight, берётся максимум, у выбранной credit -= total. Итого задача
 * получает долю процессора, пропорциональную весу кода.
 *
 * Real-time поток (SYS_setrt): если есть готовая rt-задача — выполняется ВСЕГДА
 * она; прерываний для неё нет, она отдаёт CPU только сама (yield/blocked/exit).
 */
#include <stdint.h>
#include <stddef.h>

#define TASK_MAX   64
#define KSTACK_SZ  (16 * 1024)
#define USTACK_TOP 0x7FBFFFF000ull
#define USTACK_SZ  (256 * 1024)

enum tstate { T_UNUSED = 0, T_READY = 1, T_RUNNING = 2, T_BLOCKED = 3, T_ZOMBIE = 4 };

/* контекст = irq-frame (сохраняется планировщиком на PIT) */
struct task_ctx {
    uint64_t rbp, rbx, r12, r13, r14, r15;
    uint64_t rip, cs, rflags, rsp, ss;
};

struct task {
    int64_t  pid, ppid;
    enum tstate state;
    struct task_ctx ctx;
    uint64_t *pml4;              /* NULL => kernel pml4 (ring0-задачи) */
    int       ring;              /* 0: ring0-приложение, 3: ring3 */
    void     *kstack_top;
    void     *ustack_top;
    uint64_t  brk;
    char      name[16];
    int64_t   weight;            /* вес кода, единица = KiB (мин 1) */
    int64_t   credit;            /* WRR-кредит */
    int       rt;                /* SYS_setrt: без прерываний, всегда получает время */
    int64_t   exit_code;
    int64_t   wait_for;          /* pid, которого ждут (SYS_waitpid) */
    int64_t   code_size;         /* размер ELF в байтах */
    int       claimed_console;   /* J4 */
    struct fs_node *cwd;         /* текущий каталог (см. fs.h) */
};

void task_init(void);

/* запуск приложений: path = /User/apps/... (ring3) или /Platform/Apps|MyOS/... (ring0) */
int64_t task_spawn(const char *path, char *const argv[], int64_t *code_size_out);
int64_t task_spawn_kernel(void (*fn)(void), const char *name); /* kthread */
void    task_exit(int64_t code);
int64_t task_current_pid(void);
struct task *task_current(void);

/* планировщик */
void sched_tick(void);       /* от PIT: сохранить контекст, выбрать следующую */
struct task *sched_pick(void);
void sched_set_weight(int64_t pid, int64_t weight);
void sched_set_rt(int64_t pid, int on);

/* wait/sleep */
int64_t task_waitpid(int64_t pid);
void    task_sleep(uint64_t ms);
int64_t task_wait_key(void); /* блокирующее чтение клавиши */

/* ps */
int task_fill_ps(struct ench_ps *out);

/* консольный захват (J4) */
void task_console_claim(void);
void task_console_release(void);
