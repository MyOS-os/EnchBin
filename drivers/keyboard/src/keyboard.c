// drivers/keyboard/src/keyboard.c — .b_d-драйвер PS/2 клавиатуры.
// Ставит хук в cpu.c (IRQ1, cpu_set_kbd_hook), переводит сканкоды set 1
// в ASCII (раскладка US, shift/capslock) и пушит в очередь ядра (kinput_push).
// Экспорт: ench_drv_init (конвенция .b_d), регистрация через drv_register.
#include <ench/kernel.h>
#include <ench/kcpu.h>

static int shift, caps;

// сканкоды set 1 (make), раскладка US
static const char kmap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x27] = ';', [0x28] = '\'', [0x29] = '`', [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' ',
};

static const char kmap_shift[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+',
    [0x1A] = '{', [0x1B] = '}',
    [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|',
    [0x33] = '<', [0x34] = '>', [0x35] = '?',
};

// вызывается из ISR IRQ1: только быстрые действия (очередь без блокировок)
static void on_scan(u8 sc) {
    if (sc & 0x80) {                      // отпускание
        u8 c = (u8)(sc & 0x7F);
        if (c == 0x2A || c == 0x36) shift = 0;
        return;
    }
    if (sc == 0x2A || sc == 0x36) { shift = 1; return; } // shift
    if (sc == 0x3A) { caps = !caps; return; }            // capslock
    if (sc >= 128) return;                               // расширенные — v0.1 без них
    char c = shift ? kmap_shift[sc] : kmap[sc];
    if (!c) return;
    if (caps && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    kinput_push(c);
}

static int kb_ioctl(u32 cmd, u64 arg) {
    (void)arg;
    if (cmd == 1) { shift = 0; caps = 0; return ENCH_EOK; } // сброс модификаторов
    return ENCH_EINVAL;
}

static struct ench_drv kb_drv = {
    .name = "keyboard", .type = "input",
    .init = 0, .write = 0, .read = 0, .ioctl = kb_ioctl,
};

int ench_drv_init(void) {
    cpu_set_kbd_hook(on_scan);
    return drv_register(&kb_drv);
}
