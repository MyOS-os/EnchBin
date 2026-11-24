// kernel/src/fs/fs.c — ramfs EnchBin: ustar-модуль enchdisk.tar (только чтение)
// + write-оверлей (mkdir/создание/запись поверх). Правила имён:
//   имя с '.' в начале или суффикс ".__" => скрытое (видно только ls --a);
//   суффикс ".__" => lock: чтение -> ENCH_ELOCKED, запись/удаление -> ENCH_EACCES.
// fd 0/1/2 зарезервированы (stdin/stdout/stderr), файлы с 3.
#include <ench/kernel.h>
#include <ench/osstrace.h>

#define TAR_B   512
#define OV_MAX  64
#define PATH_MAX_ 112
#define FD_MAX  32

static const u8 *tar_base;
static usize tar_size;
static char cwd[96] = "/";

// ---- оверлей записи ----
struct ovnode {
    u8 used, is_dir, deleted;
    char path[PATH_MAX_];
    u8 *data;
    usize len, cap;
};
static struct ovnode ovs[OV_MAX];

// fd-таблица
struct fdesc {
    u8 used, is_ov, is_dir;
    struct ovnode *ov;
    const u8 *tdata;
    usize tsize, off;
};
static struct fdesc fds[FD_MAX];

// ---- правила имён ----
static int ends_with(const char *s, const char *suf) {
    usize ls = ench_strlen(s), lf = ench_strlen(suf);
    return ls >= lf && ench_strcmp(s + ls - lf, suf) == 0;
}
i32 fs_name_locked(const char *name) { return ends_with(name, ENCH_LOCK_SUFFIX); }
i32 fs_name_hidden(const char *name) { return name[0] == '.' || ends_with(name, ENCH_LOCK_SUFFIX); }

// ---- пути ----
usize fs_path_join(char *out, usize cap, const char *dir, const char *name) {
    if (!out || !cap) return 0;
    usize l = 0;
    if (name[0] == '/') dir = "";
    usize ld = ench_strlen(dir);
    while (ld > 1 && dir[ld - 1] == '/') ld--; // убрать хвостовые '/'
    if (ld && !(ld == 1 && dir[0] == '/')) {
        for (usize i = 0; i < ld && l + 1 < cap; i++) out[l++] = dir[i];
        if (l + 1 < cap) out[l++] = '/';
    }
    for (usize i = 0; name[i] && l + 1 < cap; i++) out[l++] = name[i];
    out[l] = 0;
    return l;
}

// канонический путь: абсолютный или от cwd; '/' в конце убирается
static void norm(char *out, usize cap, const char *p) {
    char tmp[PATH_MAX_];
    if (!p || !p[0]) { out[0] = '/'; out[1] = 0; return; }
    if (p[0] == '/') {
        usize l = ench_strlen(p);
        if (l >= sizeof tmp) l = sizeof tmp - 1;
        ench_memcpy(tmp, p, l);
        tmp[l] = 0;
    } else {
        fs_path_join(tmp, sizeof tmp, cwd, p);
    }
    usize l = ench_strlen(tmp);
    while (l > 1 && tmp[l - 1] == '/') tmp[--l] = 0;
    if (l >= cap) l = cap - 1;
    ench_memcpy(out, tmp, l);
    out[l] = 0;
}

static void basename_of(const char *path, char *out, usize cap) {
    const char *nm = path;
    for (const char *p = path; *p; p++) if (*p == '/') nm = p + 1;
    usize l = ench_strlen(nm);
    if (l >= cap) l = cap - 1;
    ench_memcpy(out, nm, l);
    out[l] = 0;
}

static void dirname_of(const char *path, char *out, usize cap) {
    usize l = ench_strlen(path);
    while (l && path[l - 1] != '/') l--;
    if (l <= 1) { out[0] = '/'; out[1] = 0; return; }
    usize c = l - 1;
    if (c >= cap) c = cap - 1;
    ench_memcpy(out, path, c);
    out[c] = 0;
}

// ---- ustar ----
struct tent { const char *name; const u8 *data; usize size; int isdir; };

static int tar_next(struct tent *out, const u8 **pos) {
    const u8 *p = *pos;
    while (tar_base && p && p + TAR_B <= tar_base + tar_size) {
        if (p[0] == 0) { *pos = 0; return 0; }         // конец архива
        if (ench_strncmp((const char *)p + 257, "ustar", 5) != 0) { *pos = 0; return 0; }
        usize sz = 0;
        for (int i = 0; i < 11 && p[124 + i] >= '0' && p[124 + i] <= '7'; i++)
            sz = sz * 8 + (usize)(p[124 + i] - '0');
        *pos = p + TAR_B + ENCH_ALIGN_UP(sz, TAR_B);
        out->name = (const char *)p;
        out->data = p + TAR_B;
        out->size = sz;
        out->isdir = (p[156] == '5');
        return 1;
    }
    return 0;
}

static int tar_lookup(const char *path, int *isdir, const u8 **data, usize *size) {
    const u8 *p = tar_base;
    struct tent e;
    while (p && tar_next(&e, &p)) {
        if (ench_strcmp(e.name, path + 1) == 0) { // path ведёт с '/'
            *isdir = e.isdir; *data = e.data; *size = e.size;
            return 1;
        }
    }
    return 0;
}

// ---- оверлей ----
static struct ovnode *ov_find(const char *path) {
    for (int i = 0; i < OV_MAX; i++)
        if (ovs[i].used && !ovs[i].deleted && ench_strcmp(ovs[i].path, path) == 0)
            return &ovs[i];
    return 0;
}

static struct ovnode *ov_create(const char *path, int is_dir) {
    for (int i = 0; i < OV_MAX; i++) {
        if (ovs[i].used) continue;
        struct ovnode *o = &ovs[i];
        ench_memset(o, 0, sizeof *o);
        o->used = 1;
        o->is_dir = (u8)is_dir;
        usize l = ench_strlen(path);
        if (l >= PATH_MAX_) l = PATH_MAX_ - 1;
        ench_memcpy(o->path, path, l);
        o->path[l] = 0;
        return o;
    }
    return 0;
}

// ---- существование ----
static int fs_exists(const char *path) { // path с ведущим '/'
    if (path[1] == 0) return 1;          // корень
    int d; const u8 *dd; usize sz;
    if (tar_lookup(path, &d, &dd, &sz)) return 1;
    return ov_find(path) != 0;
}

static int is_dir_path(const char *path) {
    if (path[1] == 0) return 1;
    int d = 0; const u8 *dd; usize sz;
    if (tar_lookup(path, &d, &dd, &sz)) return d;
    struct ovnode *o = ov_find(path);
    return o && o->is_dir;
}

// ---- init ----
void fs_init(struct limine_file *tarball) {
    tar_base = tarball ? (const u8 *)tarball->address : 0;
    tar_size = tarball ? (usize)tarball->size : 0;
    ench_memset(ovs, 0, sizeof ovs);
    ench_memset(fds, 0, sizeof fds);
    cwd[0] = '/'; cwd[1] = 0;
    OssTrace_FileLog("fs: tar=%p size=%zu", tar_base, tar_size);
}

// ---- fd ----
static i32 fd_alloc(void) {
    for (int i = 3; i < FD_MAX; i++)
        if (!fds[i].used) { ench_memset(&fds[i], 0, sizeof fds[i]); return i; }
    return ENCH_ENOSPC;
}

i32 fs_open(const char *path0, int mode) {
    char path[PATH_MAX_], base[64];
    norm(path, sizeof path, path0);
    basename_of(path, base, sizeof base);

    int locked = fs_name_locked(base);
    if (locked && !(mode & ENCH_O_WRITE)) return ENCH_ELOCKED;  // cat .pass.__ -> err
    if (locked && (mode & (ENCH_O_WRITE | ENCH_O_CREATE))) return ENCH_EACCES;

    struct ovnode *ov = ov_find(path);
    if (ov && ov->is_dir) return ENCH_EISDIR;
    int tdir = 0; const u8 *tdata = 0; usize tsize = 0;
    int tarhit = tar_lookup(path, &tdir, &tdata, &tsize);
    if (tarhit && tdir) return ENCH_EISDIR;

    i32 fd = fd_alloc();
    if (fd < 0) return fd;

    if (mode & ENCH_O_WRITE) {
        if (!ov && !tarhit && !(mode & ENCH_O_CREATE)) return ENCH_ENOENT;
        if (!ov) {
            ov = ov_create(path, 0);
            if (!ov) return ENCH_ENOSPC;
            if (tarhit && tsize && (mode & ENCH_O_APPEND)) {
                // copy-on-write: перенос содержимого tar-файла
                u8 *mem = krealloc(0, tsize);
                if (!mem) return ENCH_ENOSPC;
                ench_memcpy(mem, tdata, tsize);
                ov->data = mem;
                ov->len = ov->cap = tsize;
            }
        }
        if (!(mode & ENCH_O_APPEND)) { ov->len = 0; fds[fd].off = 0; }
        else fds[fd].off = ov->len;
        fds[fd].used = 1;
        fds[fd].is_ov = 1;
        fds[fd].ov = ov;
        return fd;
    }

    // чтение
    if (ov) {
        fds[fd].used = 1;
        fds[fd].is_ov = 1;
        fds[fd].ov = ov;
        return fd;
    }
    if (tarhit) {
        fds[fd].used = 1;
        fds[fd].tdata = tdata;
        fds[fd].tsize = tsize;
        return fd;
    }
    fds[fd].used = 0;
    return ENCH_ENOENT;
}

i32 fs_read(i32 fd, void *buf, usize n) {
    if (fd < 3 || fd >= FD_MAX || !fds[fd].used) return ENCH_EBADF;
    if (fds[fd].is_dir) return ENCH_EISDIR;
    const u8 *src;
    usize avail;
    if (fds[fd].is_ov) {
        if (fds[fd].ov->is_dir) return ENCH_EISDIR;
        src = fds[fd].ov->data;
        avail = fds[fd].ov->len;
    } else {
        src = fds[fd].tdata;
        avail = fds[fd].tsize;
    }
    if (!src || fds[fd].off >= avail || !n) return 0;
    usize got = ENCH_MIN(n, avail - fds[fd].off);
    ench_memcpy(buf, src + fds[fd].off, got);
    fds[fd].off += got;
    return (i32)got;
}

i32 fs_write(i32 fd, const void *buf, usize n) {
    if (fd < 3 || fd >= FD_MAX || !fds[fd].used) return ENCH_EBADF;
    if (!fds[fd].is_ov) return ENCH_EACCES;   // tar-слой только для чтения
    struct ovnode *ov = fds[fd].ov;
    if (ov->is_dir) return ENCH_EISDIR;
    if (fds[fd].off + n > ov->cap) {
        usize nc = ENCH_ALIGN_UP(fds[fd].off + n + 1, 256);
        u8 *nd = krealloc(ov->data, nc);
        if (!nd) return ENCH_ENOSPC;
        ov->data = nd;
        ov->cap = nc;
    }
    if (n) ench_memcpy(ov->data + fds[fd].off, buf, n);
    fds[fd].off += n;
    if (fds[fd].off > ov->len) ov->len = fds[fd].off;
    return (i32)n;
}

void fs_close(i32 fd) {
    if (fd >= 3 && fd < FD_MAX) fds[fd].used = 0;
}

i32 fs_stat(const char *path0, struct ench_fstat *st) {
    if (!st) return ENCH_EINVAL;
    char path[PATH_MAX_], base[64];
    norm(path, sizeof path, path0);
    basename_of(path, base, sizeof base);
    ench_memset(st, 0, sizeof *st);
    usize l = ench_strlen(base);
    if (l >= sizeof st->name) l = sizeof st->name - 1;
    ench_memcpy(st->name, base, l);
    st->hidden = (u8)fs_name_hidden(base);
    st->locked = (u8)fs_name_locked(base);
    if (path[1] == 0) { st->is_dir = 1; return ENCH_EOK; }
    int d = 0; const u8 *dd; usize sz = 0;
    if (tar_lookup(path, &d, &dd, &sz)) {
        st->is_dir = (u8)d;
        st->size = (i64)sz;
        return ENCH_EOK;
    }
    struct ovnode *o = ov_find(path);
    if (o) {
        st->is_dir = o->is_dir;
        st->size = (i64)o->len;
        return ENCH_EOK;
    }
    return ENCH_ENOENT;
}

i32 fs_mkdir(const char *path0) {
    char path[PATH_MAX_];
    norm(path, sizeof path, path0);
    if (path[1] == 0) return ENCH_EEXIST;
    if (fs_exists(path)) return ENCH_EEXIST;
    if (!ov_create(path, 1)) return ENCH_ENOSPC;
    return ENCH_EOK;
}

i32 fs_unlink(const char *path0) {
    char path[PATH_MAX_], base[64];
    norm(path, sizeof path, path0);
    basename_of(path, base, sizeof base);
    if (fs_name_locked(base)) return ENCH_EACCES;
    if (path[1] == 0) return ENCH_EACCES;
    struct ovnode *o = ov_find(path);
    if (o) { o->deleted = 1; return ENCH_EOK; }
    int d; const u8 *dd; usize sz;
    if (tar_lookup(path, &d, &dd, &sz)) return ENCH_EPERM; // tar-слой неизменяем
    return ENCH_ENOENT;
}

// ---- listdir: "имя\n", дедуп, скрытые только с show_hidden ----
#define SEEN_N 96
#define SEEN_L 64
static char seen[SEEN_N][SEEN_L];
static int nseen;

static void seen_reset(void) { nseen = 0; }
static int seen_has(const char *name) {
    for (int i = 0; i < nseen; i++)
        if (ench_strcmp(seen[i], name) == 0) return 1;
    return 0;
}
static void seen_add(const char *name) {
    if (nseen >= SEEN_N) return;
    usize l = ench_strlen(name);
    if (l >= SEEN_L) l = SEEN_L - 1;
    ench_memcpy(seen[nseen], name, l);
    seen[nseen][l] = 0;
    nseen++;
}

static int parent_is(const char *entry, const char *dir) { // dir с ведущим '/'
    usize dl = ench_strlen(dir);
    if (dl == 1) {                                     // корень
        for (; *entry; entry++) if (*entry == '/') return 0;
        return entry[0] != 0;
    }
    if (ench_strncmp(entry, dir + 1, dl - 1) != 0) return 0;
    const char *rest = entry + (dl - 1);
    if (*rest != '/') return 0;
    rest++;
    if (!*rest) return 0;
    for (; *rest; rest++) if (*rest == '/') return 0;
    return 1;
}

static void list_put(char *out, usize cap, usize *o, i32 *cnt, const char *name, int show_hidden) {
    if (ench_strcmp(name, ".") == 0 || ench_strcmp(name, "..") == 0) return;
    if (!show_hidden && fs_name_hidden(name)) return;
    if (seen_has(name)) return;
    seen_add(name);
    for (usize i = 0; name[i] && *o + 1 < cap; i++) out[(*o)++] = name[i];
    if (*o + 1 < cap) out[(*o)++] = '\n';
    (*cnt)++;
}

i32 fs_listdir(const char *path0, char *out, usize cap, int show_hidden) {
    char dir[PATH_MAX_];
    norm(dir, sizeof dir, path0);
    if (!is_dir_path(dir)) return ENCH_ENOTDIR;
    if (!out || !cap) return ENCH_EINVAL;
    seen_reset();
    usize o = 0;
    i32 cnt = 0;

    // tar-слой
    const u8 *p = tar_base;
    struct tent e;
    while (p && tar_next(&e, &p)) {
        if (!parent_is(e.name, dir)) continue;
        char base[64];
        basename_of(e.name, base, sizeof base);
        list_put(out, cap, &o, &cnt, base, show_hidden);
    }
    // оверлей (удалённые пропускаются; tar-файлы неизменяемы и удаляться не могут)
    for (int i = 0; i < OV_MAX; i++) {
        if (!ovs[i].used || ovs[i].deleted) continue;
        if (!parent_is(ovs[i].path, dir)) continue;
        char base[64];
        basename_of(ovs[i].path, base, sizeof base);
        list_put(out, cap, &o, &cnt, base, show_hidden);
    }
    if (o < cap) out[o] = 0;
    return cnt;
}

i32 fs_chdir(const char *path0) {
    char path[PATH_MAX_];
    norm(path, sizeof path, path0);
    if (!is_dir_path(path)) return ENCH_ENOTDIR;
    usize l = ench_strlen(path);
    if (l >= sizeof cwd) return ENCH_EINVAL;
    ench_memcpy(cwd, path, l + 1);
    return ENCH_EOK;
}

usize fs_getcwd(char *out, usize cap) {
    if (!out || !cap) return 0;
    usize l = ench_strlen(cwd);
    if (l >= cap) l = cap - 1;
    ench_memcpy(out, cwd, l);
    out[l] = 0;
    return l;
}

// ---- filelog (OssTrace_FileLog): ENCH_LOG_DIR/<app>.log ----
i32 fs_log_write(const char *app, const char *line) {
    char nm[40], path[PATH_MAX_];
    usize l = ench_strlen(app);
    if (l >= sizeof nm - 5) l = sizeof nm - 5;
    ench_memcpy(nm, app, l);
    nm[l] = 0;
    ench_memcpy(nm + l, ".log", 5);
    fs_path_join(path, sizeof path, ENCH_LOG_DIR, nm);
    struct ovnode *ov = ov_find(path);
    if (!ov) {
        ov = ov_create(path, 0);
        if (!ov) return ENCH_ENOSPC;
    }
    usize ll = ench_strlen(line);
    if (ov->len + ll + 2 > ov->cap) {
        usize nc = ENCH_ALIGN_UP(ov->len + ll + 2, 256);
        u8 *nd = krealloc(ov->data, nc);
        if (!nd) return ENCH_ENOSPC;
        ov->data = nd;
        ov->cap = nc;
    }
    ench_memcpy(ov->data + ov->len, line, ll);
    ov->data[ov->len + ll] = '\n';
    ov->len += ll + 1;
    return ENCH_EOK;
}

// ---- kernel-внутреннее чтение без lock-проверок (.pass.__) ----
i32 fs_raw_read(const char *path0, void *buf, usize cap) {
    char path[PATH_MAX_];
    norm(path, sizeof path, path0);
    struct ovnode *o = ov_find(path);
    if (o) {
        if (o->is_dir) return ENCH_EISDIR;
        usize got = ENCH_MIN(cap, o->len);
        if (got) ench_memcpy(buf, o->data, got);
        return (i32)got;
    }
    int d; const u8 *dd; usize sz;
    if (tar_lookup(path, &d, &dd, &sz)) {
        if (d) return ENCH_EISDIR;
        usize got = ENCH_MIN(cap, sz);
        if (got) ench_memcpy(buf, dd, got);
        return (i32)got;
    }
    return ENCH_ENOENT;
}
