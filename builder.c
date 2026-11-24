/* builder.c — единая точка сборки EnchBin (хост-инструмент).
 *
 * Читает scripts/build.json и вызывает цели scripts/Makefile.
 * Результаты сборки кладёт в build/ и build/disk (в репозиторий не попадают:
 * политика — только исходники).
 *
 * Запуск:
 *   cc -O2 -o bin/builder builder.c
 *   bin/builder build          - всё (ядро, модуль, драйверы, приложения, ISO)
 *   bin/builder kernel|module|drivers|apps|stdlib|disk|iso|run|clean
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUILD_JSON "scripts/build.json"
#define MAKEFILE   "scripts/Makefile"

/* ---------- мини-JSON ---------- */
typedef struct JN {
    int t; /* 0=obj 1=arr 2=str 3=num 4=bool */
    char* s; double n;
    struct JN** kids; char** keys; int nk;
} JN;

static char* jb; static size_t jp;

static JN* jnew(int t){ JN* n = calloc(1, sizeof(JN)); n->t = t; return n; }
static void jskip(void){ while (jb[jp]==' '||jb[jp]=='\t'||jb[jp]=='\n'||jb[jp]=='\r') jp++; }
static char* jlit(void){
    jskip(); if (jb[jp] != '"') return NULL; jp++;
    size_t cap = 32, len = 0; char* out = malloc(cap);
    while (jb[jp] && jb[jp] != '"') {
        char c = jb[jp++];
        if (c == '\\') { char e = jb[jp++]; c = (e=='n')?'\n':(e=='t')?'\t':e; }
        if (len + 2 > cap) { cap *= 2; out = realloc(out, cap); }
        out[len++] = c;
    }
    if (jb[jp] == '"') jp++;
    out[len] = 0; return out;
}
static JN* jparse(void){
    jskip(); char c = jb[jp];
    if (c == '{') {
        jp++; JN* n = jnew(0); int cap = 8;
        jskip(); if (jb[jp] == '}') { jp++; return n; }
        for (;;) {
            char* k = jlit(); jskip(); if (jb[jp] == ':') jp++;
            JN* v = jparse();
            if (n->nk == cap) { cap *= 2; n->kids = realloc(n->kids, cap*sizeof(JN*)); n->keys = realloc(n->keys, cap*sizeof(char*)); }
            n->keys[n->nk] = k; n->kids[n->nk++] = v;
            jskip(); if (jb[jp] == ',') { jp++; continue; }
            if (jb[jp] == '}') { jp++; break; }
            break;
        }
        return n;
    }
    if (c == '[') {
        jp++; JN* n = jnew(1); int cap = 8;
        jskip(); if (jb[jp] == ']') { jp++; return n; }
        for (;;) {
            JN* v = jparse();
            if (n->nk == cap) { cap *= 2; n->kids = realloc(n->kids, cap*sizeof(JN*)); }
            n->kids[n->nk++] = v;
            jskip(); if (jb[jp] == ',') { jp++; continue; }
            if (jb[jp] == ']') { jp++; break; }
            break;
        }
        return n;
    }
    if (c == '"') { JN* n = jnew(2); n->s = jlit(); return n; }
    if (!strncmp(jb+jp, "true", 4))  { jp += 4; JN* n = jnew(4); n->n = 1; return n; }
    if (!strncmp(jb+jp, "false", 5)) { jp += 5; return jnew(4); }
    { JN* n = jnew(3); char* e; n->n = strtod(jb+jp, &e); jp = e - jb; return n; }
}
static JN* jroot(void){
    FILE* f = fopen(BUILD_JSON, "rb");
    if (!f) { fprintf(stderr, "builder: нет %s\n", BUILD_JSON); exit(1); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    jb = malloc(sz+1); fread(jb, 1, sz, f); jb[sz] = 0; fclose(f);
    jp = 0; return jparse();
}
static JN* jget(JN* o, const char* k){
    if (!o || o->t != 0) return NULL;
    for (int i = 0; i < o->nk; i++) if (!strcmp(o->keys[i], k)) return o->kids[i];
    return NULL;
}
static JN* jidx(JN* a, int i){ return (a && a->t == 1 && i < a->nk) ? a->kids[i] : NULL; }
static const char* jstr(JN* n){ return (n && n->t == 2) ? n->s : ""; }
static const char* jat(JN* o, const char* k){ return jstr(jget(o, k)); }

/* ---------- утилиты ---------- */
static void runf(const char* fmt, ...){
    char cmd[8192]; va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    printf("[builder] %s\n", cmd);
    if (system(cmd) != 0) { fprintf(stderr, "builder: СБОЙ: %s\n", cmd); exit(2); }
}
static void mkdirs(const char* p){
    char tmp[1024]; snprintf(tmp, sizeof tmp, "%s", p);
    for (char* q = tmp + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
    mkdir(tmp, 0755);
}
static void collect_c(const char* dir, char* out, size_t cap){
    DIR* d = opendir(dir); if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char path[1024]; snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat st; if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) collect_c(path, out, cap);
        else {
            size_t l = strlen(e->d_name);
            if (l > 2 && !strcmp(e->d_name + l - 2, ".c")) {
                size_t cl = strlen(out);
                snprintf(out + cl, cap - cl, "%s ", path);
            }
        }
    }
    closedir(d);
}
static void collect_list(JN* src, char* out, size_t cap){
    if (!src) return;
    if (src->t == 2) { snprintf(out, cap, "%s", src->s); return; }
    size_t o = 0; out[0] = 0;
    for (int i = 0; i < src->nk; i++) { o += snprintf(out + o, cap - o, "%s%s", i ? " " : "", src->kids[i]->t == 2 ? src->kids[i]->s : ""); }
}
static void copytree(const char* src, const char* dst){
    DIR* d = opendir(src); if (!d) { mkdirs(dst); return; }
    mkdirs(dst);
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char sp[1024], dp[1024];
        snprintf(sp, sizeof sp, "%s/%s", src, e->d_name);
        snprintf(dp, sizeof dp, "%s/%s", dst, e->d_name);
        struct stat st; if (stat(sp, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) copytree(sp, dp);
        else {
            FILE* in = fopen(sp, "rb"); if (!in) continue;
            FILE* ou = fopen(dp, "wb"); if (ou) {
                char buf[8192]; size_t r;
                while ((r = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, r, ou);
                fclose(ou);
            }
            fclose(in);
        }
    }
    closedir(d);
}
static void place(const char* what, const char* rel){
    char dst[1024]; snprintf(dst, sizeof dst, "build/disk/%s", rel);
    char ddir[1024]; snprintf(ddir, sizeof ddir, "%s", dst);
    char* sl = strrchr(ddir, '/'); if (sl) { *sl = 0; mkdirs(ddir); }
    runf("cp %s %s", what, dst);
}

/* ---------- tar (ustar) ---------- */
static void tar_add(FILE* f, const char* name, const char* srcpath, int isdir){
    struct stat st; stat(srcpath, &st);
    unsigned char hdr[512]; memset(hdr, 0, sizeof hdr);
    const char* nm = name[0] ? name : ".";
    size_t nl = strlen(nm); if (nl > 99) nl = 99;
    memcpy(hdr, nm, nl);
    strcpy((char*)hdr + 100, "0000644");
    strcpy((char*)hdr + 108, "0000000");
    strcpy((char*)hdr + 116, "0000000");
    char szb[13]; unsigned long long sz = isdir ? 0 : (unsigned long long)st.st_size;
    snprintf(szb, sizeof szb, "%011llo", sz); memcpy(hdr + 124, szb, 11);
    strcpy((char*)hdr + 136, "00000000000");
    hdr[156] = isdir ? '5' : '0';
    strcpy((char*)hdr + 257, "ustar"); hdr[263] = '0'; hdr[264] = '0';
    memset(hdr + 148, ' ', 8);
    unsigned long long chk = 0;
    for (int i = 0; i < 512; i++) chk += hdr[i];
    snprintf((char*)hdr + 148, 8, "%06llo", chk); hdr[154] = 0; hdr[155] = ' ';
    fwrite(hdr, 1, 512, f);
    if (!isdir) {
        FILE* in = fopen(srcpath, "rb"); if (in) {
            char buf[8192]; size_t r, total = 0;
            while ((r = fread(buf, 1, sizeof buf, in)) > 0) { fwrite(buf, 1, r, f); total += r; }
            fclose(in);
            unsigned long long pad = (512 - (total % 512)) % 512;
            static const char z[512] = {0};
            if (pad) fwrite(z, 1, pad, f);
        }
    }
}
static void tar_dir(FILE* f, const char* base, const char* rel){
    char full[1024]; snprintf(full, sizeof full, "%s/%s", base, rel);
    DIR* d = opendir(full); if (!d) return;
    tar_add(f, rel[0] ? rel : ".", full, 1);
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char nrel[1024]; snprintf(nrel, sizeof nrel, "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        char sp[1024]; snprintf(sp, sizeof sp, "%s/%s", base, nrel);
        struct stat st; if (stat(sp, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) tar_dir(f, base, nrel);
        else tar_add(f, nrel, sp, 0);
    }
    closedir(d);
}

/* ---------- шаги сборки ---------- */
static JN* R;

static void makecall(const char* tgt, const char* vars){
    runf("make -f %s %s %s", MAKEFILE, tgt, vars ? vars : "");
}

static void step_stdlib_c(void){
    JN* st = jget(jget(R, "stdlib"), "c");
    char srcs[4096] = ""; collect_c(jat(st, "src"), srcs, sizeof srcs);
    mkdirs("build/obj");
    char objs[4096] = ""; size_t o = 0;
    char* s = srcs;
    while (*s) {
        char* e = strchr(s, ' '); if (e) *e = 0;
        char obj[256]; const char* bn = strrchr(s, '/'); bn = bn ? bn + 1 : s;
        snprintf(obj, sizeof obj, "build/obj/lib_%s.o", bn);
        makecall("obj_u", "SRC='%s' OBJ='%s'", s, obj);
        o += snprintf(objs + o, sizeof objs - o, "%s ", obj);
        s = e ? e + 1 : s + strlen(s);
    }
    makecall("ar", "OBJS='%s' OUT='%s'", objs, jat(st, "ar"));
    printf("[builder] stdlib c: libench готов\n");
}

static void step_module(void){
    JN* m = jget(R, "module");
    mkdirs("build/obj");
    makecall("obj_p", "SRC='%s' OBJ='build/obj/drive_mod.o'", jat(m, "src"));
    makecall("link_reloc", "OBJS='build/obj/drive_mod.o' OUT='%s'", jat(m, "out"));
    printf("[builder] модуль %s готов\n", jat(m, "out"));
}

static void step_kernel(void){
    JN* k = jget(R, "kernel");
    char srcs[8192] = ""; collect_c(jat(k, "src"), srcs, sizeof srcs);
    JN* extra = jget(k, "extra");
    if (extra) for (int i = 0; i < extra->nk; i++) {
        size_t l = strlen(srcs); snprintf(srcs + l, sizeof srcs - l, "%s ", extra->kids[i]->s);
    }
    mkdirs("build/obj");
    char objs[16384] = ""; size_t o = 0;
    char inc[512] = "";
    JN* incs = jget(k, "extra_inc");
    if (incs) for (int i = 0; i < incs->nk; i++) snprintf(inc + strlen(inc), sizeof inc - strlen(inc), " -I%s", incs->kids[i]->s);
    char* s = srcs;
    while (*s) {
        char* e = strchr(s, ' '); if (e) *e = 0;
        char obj[256]; const char* bn = strrchr(s, '/'); bn = bn ? bn + 1 : s;
        snprintf(obj, sizeof obj, "build/obj/k_%s.o", bn);
        makecall("obj_k", "SRC='%s' OBJ='%s' EXTRA_INC='%s' EXTRA_DEF='-DENCH_KERNEL'", s, obj, inc);
        o += snprintf(objs + o, sizeof objs - o, "%s ", obj);
        s = e ? e + 1 : s + strlen(s);
    }
    makecall("embed", "DIR='build' BIN='drive.mke' OUT='build/obj/drive_mke.o'");
    o += snprintf(objs + o, sizeof objs - o, "build/obj/drive_mke.o");
    makecall("link_kernel", "LINKER='%s' OBJS='%s' OUT='%s'", jat(k, "linker"), objs, jat(k, "out"));
    printf("[builder] ядро %s готово (drive.mke встроен)\n", jat(k, "out"));
}

static void step_unit(const char* tag, JN* unit, const char* objtgt, const char* linktgt, const char* lib){
    const char* name = jat(unit, "name");
    char objs[4096] = ""; size_t o = 0;
    char inc[512] = "";
    const char* incp = jat(unit, "inc");
    if (incp && incp[0]) snprintf(inc, sizeof inc, " -I%s", incp);
    JN* src = jget(unit, "src");
    if (src && src->t == 1) {
        for (int i = 0; i < src->nk; i++) {
            char obj[256]; snprintf(obj, sizeof obj, "build/obj/%s_%d.o", name, i);
            makecall(objtgt, "SRC='%s' OBJ='%s' EXTRA_INC='%s'", src->kids[i]->s, obj, inc);
            o += snprintf(objs + o, sizeof objs - o, "%s ", obj);
        }
    } else {
        char obj[256]; snprintf(obj, sizeof obj, "build/obj/%s_0.o", name);
        makecall(objtgt, "SRC='%s' OBJ='%s' EXTRA_INC='%s'", jstr(src), obj, inc);
        snprintf(objs, sizeof objs, "%s", obj);
    }
    char out[256]; snprintf(out, sizeof out, "build/obj/%s", name);
    if (linktgt) {
        if (lib) makecall(linktgt, "LINKER='scripts/link/app.ld' OBJS='%s' LIB='%s' OUT='%s'", objs, lib, out);
        else     makecall(linktgt, "OBJS='%s' OUT='%s'", objs, out);
    }
    place(out, jat(unit, "out"));
    printf("[builder] %s %s -> %s\n", tag, name, jat(unit, "out"));
}

static void step_drivers(void){
    JN* ds = jget(R, "drivers");
    for (int i = 0; i < ds->nk; i++) step_unit("driver", ds->kids[i], "obj_p", "link_reloc", NULL);
}
static void step_apps_score(void){
    JN* as = jget(R, "apps_score");
    for (int i = 0; i < as->nk; i++) step_unit("appscore", as->kids[i], "obj_p", "link_reloc", NULL);
}
static void step_apps_user(void){
    JN* au = jget(R, "apps_user");
    const char* lib = jat(jget(jget(R, "stdlib"), "c"), "ar");
    for (int i = 0; i < au->nk; i++) step_unit("appuser", au->kids[i], "obj_u", "link_app", lib);
}

static void limine_fetch(void){
    struct stat st;
    if (stat("third_party/limine/bin/limine-bios-cd.bin", &st) == 0) return;
    mkdirs("third_party/limine/bin");
    const char* ver = getenv("LIMINE_VERSION"); if (!ver || !ver[0]) ver = "v8.0.0";
    printf("[builder] скачиваю limine %s (или положите файлы в third_party/limine/bin вручную)\n", ver);
    char url[512];
    snprintf(url, sizeof url,
        "curl -fL https://github.com/limine-bootloader/limine-release/releases/download/%s/limine-%s-binall.tar.gz"
        " -o third_party/limine/binall.tar.gz", ver, ver);
    if (system(url) != 0) {
        snprintf(url, sizeof url,
            "curl -fL https://github.com/limine-bootloader/limine-release/releases/download/%s/limine-binall.tar.gz"
            " -o third_party/limine/binall.tar.gz", ver);
        if (system(url) != 0) {
            fprintf(stderr, "builder: не удалось скачать limine. Скачайте release и распакуйте в third_party/limine/bin\n");
            exit(3);
        }
    }
    runf("tar -xzf third_party/limine/binall.tar.gz -C third_party/limine");
}

static void step_disk(int do_iso){
    JN* dk = jget(R, "disk");
    const char* stage = jat(dk, "stage");
    runf("rm -rf %s", stage); mkdirs(stage);
    copytree(jat(dk, "root"), stage);
    place(jat(jget(R, "kernel"), "out"), jat(dk, "kernel_in_iso"));
    char conf[1024]; snprintf(conf, sizeof conf, "%s/limine.conf", stage);
    runf("cp %s %s", jat(dk, "limine_conf"), conf);
    limine_fetch();
    char tar[256]; snprintf(tar, sizeof tar, "%s/enchdisk.tar", stage);
    FILE* f = fopen(tar, "wb"); if (!f) { fprintf(stderr, "builder: tar\n"); exit(2); }
    tar_dir(f, stage, "");
    static const char z[1024] = {0}; fwrite(z, 1, 1024, f);
    fclose(f);
    printf("[builder] диск упакован: %s\n", tar);
    if (do_iso) {
        runf("xorriso -as mkisofs -b third_party/limine/bin/limine-bios-cd.bin -no-emul-boot"
             " -boot-load-size 4 -boot-info-table --protective-msdos-label"
             " -o %s %s", jat(dk, "iso"), stage);
        runf("third_party/limine/bin/limine bios-install %s", jat(dk, "iso"));
        printf("[builder] ISO готов: %s\n", jat(dk, "iso"));
    }
}

static void cmd_run(void){
    JN* run = jget(R, "run");
    runf("%s -m %s -cdrom %s", jat(run, "qemu"), jat(run, "mem"), jat(jget(R, "disk"), "iso"));
}

static void cmd_clean(void){
    runf("rm -rf build bin/obj");
    printf("[builder] чисто\n");
}

int main(int argc, char** argv){
    R = jroot();
    const char* what = argc > 1 ? argv[1] : "build";
    if (!strcmp(what, "clean"))  { cmd_clean(); return 0; }
    if (!strcmp(what, "run"))    { step_disk(1); cmd_run(); return 0; }
    if (!strcmp(what, "disk"))   { step_disk(0); return 0; }
    if (!strcmp(what, "iso"))    { step_disk(1); return 0; }
    if (!strcmp(what, "kernel")) { step_module(); step_kernel(); return 0; }
    if (!strcmp(what, "module")) { step_module(); return 0; }
    if (!strcmp(what, "drivers")){ step_drivers(); return 0; }
    if (!strcmp(what, "stdlib")) { step_stdlib_c(); return 0; }
    if (!strcmp(what, "apps"))   { step_stdlib_c(); step_apps_score(); step_apps_user(); return 0; }
    /* build = всё */
    mkdirs("build/obj");
    step_stdlib_c();
    step_module();
    step_kernel();
    step_drivers();
    step_apps_score();
    step_apps_user();
    step_disk(1);
    printf("[builder] EnchBin собран. Только исходники в репозитории, артефакты в build/\n");
    return 0;
}
