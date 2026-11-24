// kernel/src/cpu/cpu.c — GDT/TSS/IDT, заглушки прерываний (inline-asm),
// PIC/PIT, syscall-вход (syscall/sysret через iret), COM1, RTC, power, ввод.
#include <ench/kernel.h>
#include <ench/kcpu.h>

// ---- порты ----
static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void cli_i(void) { __asm__ volatile("cli"); }
static inline void sti_i(void) { __asm__ volatile("sti"); }
static inline u64 rdmsr_(u32 m) { u32 lo, hi; __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return ((u64)hi << 32) | lo; }
static inline void wrmsr_(u32 m, u64 v) { __asm__ volatile("wrmsr" :: "c"(m), "a"((u32)v), "d"((u32)(v >> 32))); }

// ---- COM1 ----
static int serial_ok;
void kserial_init(void) {
    outb(0x3F8 + 1, 0x00); outb(0x3F8 + 3, 0x80); outb(0x3F8, 0x01); outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03); outb(0x3F8 + 2, 0xC7); outb(0x3F8 + 4, 0x0B); serial_ok = 1;
}
void kserial_write(const char *s, usize n) {
    if (!serial_ok) return;
    for (usize i = 0; i < n; i++) { while (!(inb(0x3F8 + 5) & 0x20)) ; outb(0x3F8, s[i]); }
}

// ---- GDT + TSS ----
struct tss {
    u32 rsv0; u64 rsp0, rsp1, rsp2; u64 rsv1; u64 ist[7]; u32 rsv2; u16 rsv3; u16 iopb;
} __attribute__((packed));
static struct tss tss;
static u64 gdt[7] = {
    0,
    0x00A09B000000FFFFull, /* 0x08 kernel code */
    0x00A093000000FFFFull, /* 0x10 kernel data */
    0x00A0FB000000FFFFull, /* 0x18 user code (селектор 0x1B) */
    0x00A0F3000000FFFFull, /* 0x20 user data (селектор 0x23) */
    0, 0                   /* 0x28: TSS (16 байт) */
};
static struct { u16 limit; u64 base; } __attribute__((packed)) gdt_desc;

static void gdt_load(void) {
    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base = (u64)gdt;
    __asm__ volatile("lgdt %0" :: "m"(gdt_desc));
    __asm__ volatile(
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n" ::: "rax", "memory");
    __asm__ volatile("movw $0x10, %%ax\n movw %%ax, %%ds\n movw %%ax, %%es\n movw %%ax, %%ss" :: : "ax");
    u64 taddr = (u64)&tss;
    gdt[5] = ((u64)sizeof(tss) - 1) | ((taddr & 0xFFFFFF) << 16) | (((taddr >> 24) & 0xFF) << 32)
           | (0x0089ull << 40) | (((taddr >> 32) & 0xFFFFull) << 48) | (((taddr >> 40) & 0xFFull) << 56);
    gdt[6] = (taddr >> 56) & 0xFF;
    __asm__ volatile("ltr %%ax" :: "a"((u16)0x28));
}

// ---- IDT ----
struct idt_ent { u16 lo; u16 sel; u8 ist; u8 type; u16 mid; u32 hi; u32 z; } __attribute__((packed));
static struct idt_ent idt[256];
static struct { u16 limit; u64 base; } __attribute__((packed)) idt_desc;

static void idt_set(int v, void *fn) {
    u64 a = (u64)fn;
    idt[v].lo = a & 0xFFFF; idt[v].sel = 0x08; idt[v].ist = 0; idt[v].type = 0x8E;
    idt[v].mid = (a >> 16) & 0xFFFF; idt[v].hi = (u32)(a >> 32); idt[v].z = 0;
}

// единая таблица адресов 34 заглушек; адреса вносятся в syscall_init
// до idt_load (символы isr_N объявлены там же через extern)
static void *isrs[34];

static void idt_load(void) {
    ench_memset(idt, 0, sizeof idt);
    for (int v = 0; v <= 33; v++) idt_set(v, isrs[v]);
    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base = (u64)idt;
    __asm__ volatile("lidt %0" :: "m"(idt_desc));
}

// ---- заглушки ISR: err(если нет), vector, rax..r15; выход через iretq ----
// cpu_isr возвращает новый %rsp (переключение задач) или 0.
__asm__(
".text\n"
".macro ISR n err\n"
"isr_\\n:\n"
"  .if \\err == 0\n"
"    pushq $0\n"
"  .endif\n"
"  pushq $\\n\n"
"  jmp isr_common\n"
".endm\n"
".irp v,0,1,2,3,4,5,6,7\n ISR \\v 0\n.endr\n"
"ISR 8 1\n ISR 9 0\n"
".irp v,10,11,12,13,14\n ISR \\v 1\n.endr\n"
"ISR 15 0\n ISR 16 0\n ISR 17 1\n"
".irp v,18,19,20,21,22,23,24,25,26,27,28,29,30,31\n ISR \\v 0\n.endr\n"
"ISR 32 0\n ISR 33 0\n"
"isr_common:\n"
"  pushq %rax\n pushq %rbx\n pushq %rcx\n pushq %rdx\n pushq %rsi\n pushq %rdi\n pushq %rbp\n"
"  pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %r12\n pushq %r13\n pushq %r14\n pushq %r15\n"
"  movq %rsp, %rdi\n"
"  call cpu_isr\n"
"  testq %rax, %rax\n"
"  jz 1f\n"
"  movq %rax, %rsp\n"
"1:\n"
"  popq %r15\n popq %r14\n popq %r13\n popq %r12\n popq %r11\n popq %r10\n popq %r9\n popq %r8\n"
"  popq %rbp\n popq %rdi\n popq %rsi\n popq %rdx\n popq %rcx\n popq %rbx\n popq %rax\n"
"  addq $16, %rsp\n"
"  iretq\n"
);

// ---- syscall-вход (кадр как у ISR -> единый iret-выход) ----
u64 user_rsp_slot, cur_ksp;
void (*kbd_hook)(u8 sc);

// ---- обработчик прерываний/исключений (адреса всех isr_N объявлены
// extern в syscall_init и внесены в isrs до idt_load) ----
void *cpu_isr(struct cpu_frame *fr);
void *cpu_isr(struct cpu_frame *fr) {
    if (fr->vector == 32) {          // PIT 1000 Гц
        uptime_ticks++;
        outb(0x20, 0x20);
        return sched_timer(fr);      // переключение задач (взвешенный RR + rt)
    }
    if (fr->vector == 33) {          // PS/2 клавиатура
        u8 sc = inb(0x60);
        outb(0x20, 0x20);
        if (kbd_hook) kbd_hook(sc);
        return 0;
    }
    if (fr->vector >= 40) outb(0xA0, 0x20);
    if (fr->vector > 33 && fr->vector < 40) outb(0x20, 0x20);
    if (fr->vector < 32) {           // исключение: паника
        cli_i();
        kprintf("panic: exception %d err=%p rip=%p rsp=%p\n",
                (int)fr->vector, fr->err, fr->rip, fr->rsp);
        power_halt();
    }
    return 0;
}

// ---- syscall-вход ----
void cpu_set_kbd_hook(void (*fn)(u8)) { kbd_hook = fn; }
void cpu_set_ksp(u64 ksp) { cur_ksp = ksp; }
void cpu_set_tss_rsp0(u64 rsp) { tss.rsp0 = rsp; }

__asm__(
".text\n"
".globl syscall_entry\n"
"syscall_entry:\n"
"  movq %rsp, user_rsp_slot(%rip)\n"
"  movq cur_ksp(%rip), %rsp\n"
"  pushq $0x23\n"
"  pushq user_rsp_slot(%rip)\n"
"  pushq %r11\n"
"  pushq $0x1B\n"
"  pushq %rcx\n"
"  pushq $0\n"
"  pushq $0x80\n"
"  pushq %rax\n pushq %rbx\n pushq %rcx\n pushq %rdx\n pushq %rsi\n pushq %rdi\n pushq %rbp\n"
"  pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %r12\n pushq %r13\n pushq %r14\n pushq %r15\n"
"  movq %rsp, %rdi\n"
"  call sys_dispatch\n"
"  testq %rax, %rax\n"
"  jz 1f\n"
"  movq %rax, %rsp\n"
"1:\n"
"  popq %r15\n popq %r14\n popq %r13\n popq %r12\n popq %r11\n popq %r10\n popq %r9\n popq %r8\n"
"  popq %rbp\n popq %rdi\n popq %rsi\n popq %rdx\n popq %rcx\n popq %rbx\n popq %rax\n"
"  addq $16, %rsp\n"
"  iretq\n"
);

// ---- построение начального кадра задачи ----
void *cpu_make_frame(u64 kstack_top, u64 rip, u64 rsp, u8 user) {
    u64 fp = (kstack_top - sizeof(struct cpu_frame)) & ~15ull;
    struct cpu_frame *f = (struct cpu_frame *)fp;
    ench_memset(f, 0, sizeof *f);
    f->rip = rip;
    f->cs = user ? 0x1B : 0x08;
    f->rflags = 0x202;                      // IF + reserved bit1
    f->rsp = rsp ? rsp : fp - 512;          // kernel-поток: scratch под кадром
    f->ss = user ? 0x23 : 0x10;
    f->vector = 0; f->err = 0;
    return f;
}

// ---- вход в задачу из kernel-контекста (task_exit) ----
__attribute__((naked)) void cpu_restore_frame(struct cpu_frame *fr) {
    __asm__ volatile(
        "movq %rdi, %rsp\n"
        "popq %r15\n popq %r14\n popq %r13\n popq %r12\n popq %r11\n popq %r10\n popq %r9\n popq %r8\n"
        "popq %rbp\n popq %rdi\n popq %rsi\n popq %rdx\n popq %rcx\n popq %rbx\n popq %rax\n"
        "addq $16, %rsp\n"
        "iretq\n"
    );
}

// ---- PIC/PIT ----
static void pic_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC); /* unmask IRQ0(таймер)+IRQ1(клавиатура) */
    outb(0xA1, 0xFF);
}
static void pit_init(void) {
    u16 div = (u16)(1193182 / 1000);
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF); outb(0x40, div >> 8);
}

// ---- RTC ----
static u8 cmos(u8 r) { outb(0x70, r); return inb(0x71); }
static u8 bcd(u8 v) { return (u8)((v & 0x0F) + (v >> 4) * 10); }
void rtc_read(struct ench_rtc *o) {
    while (cmos(0x0A) & 0x80) ;
    o->sec = bcd(cmos(0x00)); o->min = bcd(cmos(0x02)); o->hour = bcd(cmos(0x04));
    o->day = bcd(cmos(0x07)); o->mon = bcd(cmos(0x08));
    o->year = 2000 + bcd(cmos(0x09));
}

// ---- ввод (очередь клавиатуры; драйвер keyboard.b_d пушит сюда) ----
static char kq[128]; static volatile int kq_r, kq_w;
void kinput_push(char c) {
    if ((kq_w - kq_r) >= 128) return;
    kq[kq_w++ & 127] = c;
}
int kinput_pop(char *out, int block) {
    while (kq_r == kq_w) {
        if (!block) return 0;
        __asm__ volatile("sti; hlt");
    }
    *out = kq[kq_r++ & 127];
    return 1;
}

// ---- power/время ----
u64 uptime_ticks;
u64 uptime_ms(void) { return uptime_ticks; }
void power_reboot(void) {
    u8 st; do { st = inb(0x64); } while (st & 2);
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile("hlt");
}
void power_halt(void) { cli_i(); for (;;) __asm__ volatile("hlt"); }

// ---- syscall_init: GDT/TSS/IDT/PIC/PIT + MSR (EFER.SCE, STAR/LSTAR/FMASK) ----
void syscall_init(void) {
    extern char isr_0[], isr_1[], isr_2[], isr_3[], isr_4[], isr_5[], isr_6[], isr_7[];
    extern char isr_8[], isr_9[], isr_10[], isr_11[], isr_12[], isr_13[], isr_14[], isr_15[];
    extern char isr_16[], isr_17[], isr_18[], isr_19[], isr_20[], isr_21[], isr_22[], isr_23[];
    extern char isr_24[], isr_25[], isr_26[], isr_27[], isr_28[], isr_29[], isr_30[], isr_31[];
    extern char isr_32[], isr_33[];
    static char *stubs[34] = {
        isr_0, isr_1, isr_2, isr_3, isr_4, isr_5, isr_6, isr_7,
        isr_8, isr_9, isr_10, isr_11, isr_12, isr_13, isr_14, isr_15,
        isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23,
        isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31,
        isr_32, isr_33
    };
    for (int v = 0; v <= 33; v++) isrs[v] = stubs[v];
    idt_load();
    ench_memset(&tss, 0, sizeof tss);
    gdt_load();
    pic_init();
    pit_init();
    u64 efer = rdmsr_(0xC0000080);
    wrmsr_(0xC0000080, efer | 1);
    wrmsr_(0xC0000081, ((u64)0x18 << 48) | ((u64)0x08 << 32));
    extern char syscall_entry[];
    wrmsr_(0xC0000082, (u64)syscall_entry);
    wrmsr_(0xC0000084, 0x4700); /* NT|DF|TF|IF сброшены */
    sti_i();
}
