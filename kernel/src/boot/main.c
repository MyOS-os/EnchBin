// kernel/src/boot/main.c — точка входа ядра EnchBin (Limine v8 protocol).
#include <ench/kernel.h>
#include <ench/osstrace.h>

const char ench_version[] = "EnchBin OS 0.1.0 \"First Light\"";

static volatile struct limine_hhdm_request rq_hhdm = { .id = LIMINE_HHDM_REQUEST, .revision = 0, .response = 0 };
static volatile struct limine_framebuffer_request rq_fb = { .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0, .response = 0 };
static volatile struct limine_memmap_request rq_mm = { .id = LIMINE_MEMMAP_REQUEST, .revision = 0, .response = 0 };
static volatile struct limine_executable_file_request rq_exe = { .id = LIMINE_EXECUTABLE_FILE_REQUEST, .revision = 0, .response = 0 };
static volatile struct limine_module_request rq_mod = { .id = LIMINE_MODULE_REQUEST, .revision = 0, .response = 0 };

__attribute__((used, section(".limine_requests_start_marker")))
u64 limine_requests_start_marker[4] = { 0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, 0x785c6ed015d3e316, 0x181e920a7852b9d9 };
__attribute__((used, section(".limine_requests")))
u64 limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 0 };
__attribute__((used, section(".limine_requests_end_marker")))
u64 limine_requests_end_marker[2] = { 0xadc0e0531bb10d03, 0x9572709f31764c62 };

static struct limine_framebuffer *g_fb;
static u64 g_hhdm;
static struct limine_memmap_response *g_mm;
static struct limine_file *g_mod;

struct limine_framebuffer *ench_boot_fb(void) { return g_fb; }
u64 ench_boot_hhdm(void) { return g_hhdm; }
struct limine_memmap_response *ench_boot_memmap(void) { return g_mm; }
struct limine_file *ench_boot_module(void) { return g_mod; }

// Точка входа (ENTRY(ench_entry) в scripts/link/kernel.ld).
void ench_entry(void) {
    kserial_init();

    if (!LIMINE_BASE_REVISION_SUPPORTED)
        power_halt();

    if (rq_hhdm.response)  g_hhdm = rq_hhdm.response->offset;
    if (rq_fb.response && rq_fb.response->framebuffer_count)
        g_fb = rq_fb.response->framebuffers[0];
    g_mm = rq_mm.response;
    if (rq_mod.response && rq_mod.response->module_count)
        g_mod = rq_mod.response->modules[0];

    kconsole_init();
    kprintf("%s\n", ench_version);
    kprintf("OssTrace: Trace=%d AsmDump=%d FileLog=%d\n",
            EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace,
            EnchBin_RingThree_Debug_FunctionDebug_OssTrace_AsmDump,
            EnchBin_RingThree_Debug_FunctionDebug_OssTrace_FileLog);

    pmm_init(g_mm);
    vmm_init(g_hhdm);
    fs_init(g_mod);
    task_init();
    mke_init();        // модуль ядра drive.mke (секция .enchmke)
    syscall_init();    // GDT/TSS/IDT/PIC/PIT + syscall (SCE)
    initd_run();       // init.d_: драйверы .b_d + autostart (shell)

    kprintf("boot: idle\n");
    task_enter_idle();
}
