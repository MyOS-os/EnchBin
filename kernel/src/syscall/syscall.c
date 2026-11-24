// kernel/src/syscall/syscall.c — диспетчер syscalls 0..26 (см. ench/abi.h).
// Вызывается из syscall_entry (cpu.c): полный cpu_frame, SysV-аргументы
// rdi,rsi,rdx,r10,r8,r9, номер в rax. Ошибка => отрицательный errno.
// Особые случаи (exit/yield/sleep/wait/spawn-блокировки) могут ПЕРЕКЛЮЧИТЬ
// задачу: sys_dispatch возвращает кадр для iretq (обычно тот же).
#include <ench/kernel.h>
#include <ench/kcpu.h>
#include <ench/osstrace.h>

#define PG 4096ull
#define USER_LIMIT 0x0000800000000000ull // проверка ring3-указателей

static int uok(const void *p, usize n) {
    u64 v = (u64)p;
    return v && v + n > v && v + n <= USER_LIMIT;
}

// копия строки из ring3 (shared address space, toy-OS): до cap-1 байт
static usize ustr_copy(char *dst, usize cap, const char *src) {
    if (!uok(src, 1) || !cap) { if (cap) dst[0] = 0; return 0; }
    usize i = 0;
    for (; i + 1 < cap && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
    return i;
}

void *sys_dispatch(struct cpu_frame *fr) {
    u64 n = fr->rax;
    u64 a1 = fr->rdi, a2 = fr->rsi, a3 = fr->rdx, a4 = fr->r10;
    i64 ret = ENCH_EUNKNOWN;
    OssTrace_Trace("sys %llu pid=%d", n, task_current_pid());

    switch (n) {
    case SYS_read: {
        char *buf = (char *)a2;
        usize len = (usize)a3;
        if (!uok(buf, len)) { ret = ENCH_EINVAL; break; }
        if ((int)a1 == ENCH_FD_STDIN) {
            usize got = 0;
            while (got < len) {
                char c;
                if (!kinput_pop(&c, 1)) break;
                buf[got++] = c;
                if (c == '\n') break;
            }
            ret = (i64)got;
        } else {
            ret = fs_read((i32)a1, buf, len);
        }
        break;
    }
    case SYS_write: {
        const char *buf = (const char *)a2;
        usize len = (usize)a3;
        if (!uok(buf, len)) { ret = ENCH_EINVAL; break; }
        if (len > 4096) len = 4096;
        if ((int)a1 == ENCH_FD_STDOUT || (int)a1 == ENCH_FD_STDERR) {
            console_app_write(buf, len);       // J4: вывод приложения монополен
            ret = (i64)len;
        } else {
            ret = fs_write((i32)a1, buf, len);
        }
        break;
    }
    case SYS_open: {
        char path[96];
        ustr_copy(path, sizeof path, (const char *)a1);
        ret = fs_open(path, (int)a2);
        break;
    }
    case SYS_close:
        fs_close((i32)a1);
        ret = ENCH_EOK;
        break;
    case SYS_stat: {
        char path[96];
        if (!uok((void *)a2, sizeof(struct ench_fstat))) { ret = ENCH_EINVAL; break; }
        ustr_copy(path, sizeof path, (const char *)a1);
        ret = fs_stat(path, (struct ench_fstat *)a2);
        break;
    }
    case SYS_mkdir: {
        char path[96];
        ustr_copy(path, sizeof path, (const char *)a1);
        ret = fs_mkdir(path);
        break;
    }
    case SYS_unlink: {
        char path[96];
        ustr_copy(path, sizeof path, (const char *)a1);
        ret = fs_unlink(path);
        break;
    }
    case SYS_listdir: {
        char path[96];
        if (!uok((void *)a2, (usize)a3)) { ret = ENCH_EINVAL; break; }
        ustr_copy(path, sizeof path, (const char *)a1);
        ret = fs_listdir(path, (char *)a2, (usize)a3, (int)a4);
        break;
    }
    case SYS_spawn: {
        // argv копируем в kernel-буферы (до 8 строк по 63 байта)
        char kpath[96];
        const char *const *uargv = (const char *const *)a2;
        const char *kav[9];
        static char kbuf[8][64];
        int ac = 0;
        ustr_copy(kpath, sizeof kpath, (const char *)a1);
        if (uargv && uok(uargv, sizeof(char *))) {
            for (; ac < 8; ac++) {
                const char *s = uargv[ac];
                if (!s || !uok(s, 1)) break;
                ustr_copy(kbuf[ac], sizeof kbuf[ac], s);
                kav[ac] = kbuf[ac];
            }
        }
        kav[ac] = 0;
        i32 pid = 0;
        i32 r = task_spawn_elf(kpath, kav, &pid);
        ret = (r == ENCH_EOK) ? pid : r;
        break;
    }
    case SYS_exit:
        task_exit_core((i32)a1);
        return task_exit_switch();             // кадр следующей задачи
    case SYS_wait: {
        i32 pid = (i32)a1, code = 0;
        void *f = fr;
        for (;;) {
            i32 st = task_wait_check(pid, &code);
            if (st > 0) { task_reap(pid < 0 ? st : pid); ret = code; break; }
            if (st < 0) { ret = st; break; }
            task_set_block();
            void *nf = task_yield_switch(f);
            if (nf == f) __asm__ volatile("hlt");
            f = nf;
        }
        task_set_run();
        fr = (struct cpu_frame *)f;
        break;
    }
    case SYS_getpid:
        ret = task_current_pid();
        break;
    case SYS_yield:
        task_yield();
        fr = (struct cpu_frame *)task_yield_switch(fr);
        ret = ENCH_EOK;
        break;
    case SYS_sleep: {
        task_sleep_begin((u64)a1);
        void *f = fr;
        while (task_sleep_pending()) {
            void *nf = task_yield_switch(f);
            if (nf == f) __asm__ volatile("hlt");
            f = nf;
        }
        task_sleep_end();
        fr = (struct cpu_frame *)f;
        ret = ENCH_EOK;
        break;
    }
    case SYS_setweight:
        task_setweight(task_current_pid(), (u32)a1);
        ret = ENCH_EOK;
        break;
    case SYS_setrt:
        task_setrt(task_current_pid(), (int)a1);
        ret = ENCH_EOK;
        break;
    case SYS_tasklist:
        if (!uok((void *)a1, (usize)a2 * sizeof(struct ench_taskinfo))) { ret = ENCH_EINVAL; break; }
        ret = task_list((struct ench_taskinfo *)a1, (i32)a2);
        break;
    case SYS_sbrk: {
        u64 old = task_getcur_brk();
        i64 delta = (i64)a1;
        u64 nb = old + (u64)delta;
        if (delta > 0 && nb > 0x7F000000ull) { ret = ENCH_ENOMEM; break; }
        if (delta > 0) {
            for (u64 q = ENCH_ALIGN_UP(old, PG); q < nb; q += PG) {
                void *pg = pmm_alloc();
                if (!pg) { ret = ENCH_ENOMEM; break; }
                vmm_map(q, (u64)pg, 3); // W|U
            }
            if (ret == ENCH_ENOMEM) break;
        }
        task_getcur_brk_set(nb);
        ret = (i64)old;
        break;
    }
    case SYS_chdir: {
        char path[96];
        ustr_copy(path, sizeof path, (const char *)a1);
        ret = fs_chdir(path);
        break;
    }
    case SYS_getcwd:
        if (!uok((void *)a1, (usize)a2)) { ret = ENCH_EINVAL; break; }
        ret = (i64)fs_getcwd((char *)a1, (usize)a2);
        break;
    case SYS_acc_create: {
        char login[32], pass[68];
        ustr_copy(login, sizeof login, (const char *)a1);
        ustr_copy(pass, sizeof pass, (const char *)a2);
        ret = acc_create(login, pass);
        break;
    }
    case SYS_acc_login: {
        char login[32], pass[68];
        ustr_copy(login, sizeof login, (const char *)a1);
        ustr_copy(pass, sizeof pass, (const char *)a2);
        ret = acc_login(login, pass);
        break;
    }
    case SYS_whoami: {
        char *out = (char *)a1;
        usize cap = (usize)a2;
        if (!uok(out, cap)) { ret = ENCH_EINVAL; break; }
        const char *u = acc_current();
        if (!u) { ret = ENCH_EPERM; break; }
        usize l = ench_strlen(u);
        if (l >= cap) l = cap - 1;
        ench_memcpy(out, u, l);
        out[l] = 0;
        ret = ENCH_EOK;
        break;
    }
    case SYS_asmrun: {
        char name[80];
        ustr_copy(name, sizeof name, (const char *)a1);
        if (name[0] != '/') {
            char full[96];
            fs_path_join(full, sizeof full, ENCH_EASM_DIR, name);
            ret = asm_run_file(full, (i64 *)a2);
        } else {
            ret = asm_run_file(name, (i64 *)a2);
        }
        break;
    }
    case SYS_drvctl: {
        switch (a1) {
        case ENCH_DRVCTL_LIST:
            if (!uok((void *)a2, (usize)a3)) { ret = ENCH_EINVAL; break; }
            ret = drv_list((char *)a2, (usize)a3);
            break;
        case ENCH_DRVCTL_LOAD: {
            char path[96];
            ustr_copy(path, sizeof path, (const char *)a2);
            ret = drv_load(path);
            break;
        }
        case ENCH_DRVCTL_CALL: {
            char name[32];
            ustr_copy(name, sizeof name, (const char *)a2);
            ret = drv_call(name, (u32)a3, a4);
            break;
        }
        default:
            ret = ENCH_EINVAL;
        }
        break;
    }
    case SYS_time:
        if (!uok((void *)a1, sizeof(struct ench_rtc))) { ret = ENCH_EINVAL; break; }
        rtc_read((struct ench_rtc *)a1);
        ret = ENCH_EOK;
        break;
    case SYS_powerctl:
        if ((int)a1 == ENCH_POWER_REBOOT) power_reboot();
        if ((int)a1 == ENCH_POWER_HALT) power_halt();
        ret = ENCH_EINVAL;
        break;
    default:
        ret = ENCH_EUNKNOWN;
    }

    fr->rax = (u64)ret;
    return fr;
}
