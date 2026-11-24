// drivers/unix_lin/src/unix_lin.c — линейная дисциплина UNIX (.b_d, compat).
// Собирает строку из очереди ввода ядра: echo, backspace, окончание '\n'.
// ioctl:
//   1 = LINE_READ — arg = &struct ench_line_req { char *buf; usize cap; };
//       блокирующий сбор строки (hlt до клавиши), возвращает длину или err
//   2 = echo вкл/выкл (arg = 0/1)
// Буферизация повторного ввода — очередь ядра (kinput), драйвер stateless.
#include <ench/kernel.h>

struct ench_line_req { char *buf; usize cap; };

static int echo_on = 1;

static int line_read(char *buf, usize cap) {
    if (!buf || cap < 2) return ENCH_EINVAL;
    usize n = 0;
    for (;;) {
        char c;
        if (!kinput_pop(&c, 1)) return ENCH_EIO;
        if (c == '\n') {
            if (echo_on) console_app_write("\n", 1);
            buf[n] = 0;
            return (int)n;
        }
        if (c == '\b' || c == 0x7F) {
            if (n) {
                n--;
                if (echo_on) console_app_write("\b \b", 3);
            }
            continue;
        }
        if (c < 0x20 || (u8)c > 0x7E) continue;   // непечатные — игнор
        if (n + 1 >= cap) continue;               // буфер полон — лишнее не пишем
        buf[n++] = c;
        if (echo_on) console_app_write(&c, 1);
    }
}

static int ul_ioctl(u32 cmd, u64 arg) {
    switch (cmd) {
    case 1: {
        struct ench_line_req *r = (struct ench_line_req *)arg;
        if (!r) return ENCH_EINVAL;
        return line_read(r->buf, r->cap);
    }
    case 2:
        echo_on = arg ? 1 : 0;
        return ENCH_EOK;
    default:
        return ENCH_EINVAL;
    }
}

static struct ench_drv ul_drv = {
    .name = "unix_lin", .type = "compat",
    .init = 0, .write = 0, .read = 0, .ioctl = ul_ioctl,
};

int ench_drv_init(void) {
    return drv_register(&ul_drv);
}
