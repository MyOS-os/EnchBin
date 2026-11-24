// kernel/src/account/account.c — аккаунт-система EnchBin.
// Хранение: ENCH_ACC_DIR/<логин>/.pass.__ — хеш FNV-1a 64 с солью-логином
// (toy-вариант до salted-SHA-256 из roadmap). Суффикс ".__" делает файл
// скрытым и lock-файлом: cat .pass.__ -> err, SYS_read -> ENCH_ELOCKED,
// но ядро читает его через fs_raw_read (без lock-проверок).
// Первый запуск: acc_count()==0 => shell предлагает создать логин+пароль;
// после reboot — только вход.
#include <ench/kernel.h>

#define LOGIN_MAX 24
#define PASS_MAX  64

static char cur_user[LOGIN_MAX + 1];

// FNV-1a 64: соль = логин, затем ':' и пароль
static u64 acc_hash(const char *login, const char *pass) {
    u64 h = 0xcbf29ce484222325ull;
    for (const char *p = login; *p; p++) { h ^= (u8)*p; h *= 0x100000001b3ull; }
    h ^= (u8)':';
    h *= 0x100000001b3ull;
    for (const char *p = pass; *p; p++) { h ^= (u8)*p; h *= 0x100000001b3ull; }
    return h;
}

static int login_ok(const char *l) {
    usize n = ench_strlen(l);
    if (n < 1 || n > LOGIN_MAX) return 0;
    for (usize i = 0; i < n; i++) {
        char c = l[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

i32 acc_create(const char *login, const char *pass) {
    if (!login || !login_ok(login) || !pass || !pass[0] || ench_strlen(pass) > PASS_MAX)
        return ENCH_EINVAL;
    char dir[112], file[128];
    fs_path_join(dir, sizeof dir, ENCH_ACC_DIR, login);
    struct ench_fstat st;
    if (fs_stat(dir, &st) == ENCH_EOK) return ENCH_EEXIST;
    i32 r = fs_mkdir(dir);
    if (r != ENCH_EOK) return r;
    fs_path_join(file, sizeof file, dir, ENCH_PASSFILE);
    i32 fd = fs_open(file, ENCH_O_WRITE | ENCH_O_CREATE);
    if (fd < 0) return fd;
    char hex[20];
    static const char hx[] = "0123456789abcdef";
    u64 h = acc_hash(login, pass);
    for (int i = 0; i < 16; i++) hex[i] = hx[(h >> (60 - 4 * i)) & 0xF];
    hex[16] = '\n';
    hex[17] = 0;
    i32 w = fs_write(fd, hex, 17);
    fs_close(fd);
    return w == 17 ? ENCH_EOK : ENCH_EIO;
}

i32 acc_login(const char *login, const char *pass) {
    if (!login || !login_ok(login) || !pass) return ENCH_EINVAL;
    char dir[112], file[128];
    fs_path_join(dir, sizeof dir, ENCH_ACC_DIR, login);
    fs_path_join(file, sizeof file, dir, ENCH_PASSFILE);
    u8 buf[24];
    i32 r = fs_raw_read(file, buf, sizeof buf - 1);
    if (r < 16) return ENCH_EACCES;
    u64 stored = 0;
    int ok = 1;
    for (int i = 0; i < 16 && ok; i++) {
        char c = (char)buf[i];
        u64 v;
        if (c >= '0' && c <= '9') v = (u64)(c - '0');
        else if (c >= 'a' && c <= 'f') v = (u64)(10 + c - 'a');
        else if (c >= 'A' && c <= 'F') v = (u64)(10 + c - 'A');
        else ok = 0, v = 0;
        stored = stored * 16 + v;
    }
    if (!ok || stored != acc_hash(login, pass)) return ENCH_EACCES;
    acc_set_current(login);
    return ENCH_EOK;
}

i32 acc_count(void) {
    char out[512];
    i32 n = fs_listdir(ENCH_ACC_DIR, out, sizeof out, 1);
    return n < 0 ? 0 : n;
}

i32 acc_list(char *out, usize cap) {
    return fs_listdir(ENCH_ACC_DIR, out, cap, 1);
}

const char *acc_current(void) { return cur_user[0] ? cur_user : 0; }

void acc_set_current(const char *login) {
    usize n = login ? ench_strlen(login) : 0;
    if (n > LOGIN_MAX) n = LOGIN_MAX;
    ench_memcpy(cur_user, login ? login : "", n);
    cur_user[n] = 0;
}
