// kernel/src/asmexec/asmexec.c — K2: динамический ассемблер/исполнитель .easm.
// Каталог ENCH_EASM_DIR (/Platform/data/asm). Язык:
//   ; или #   — комментарий
//   name:     — метка (одна на строку)
//   mov rN, imm|rM       add/sub/mul/div rN, imm|rM
//   inc/dec rN           cmp rN, imm|rM           (флаги: меньше/равно)
//   jmp/je/jne/jl/jge метка
//   out rN  (десятичное) outc rN (символ)
//   print "текст"
//   push rN / pop rN     exit                     (результат = r0)
// Регистры r0..r9 (i64), стек 128, лимит 500k шагов.
// Вывод идёт как вывод приложения (console_app_write, J4). OssTrace_AsmDump:
// asm_dump_last печатает разбор последней программы.
#include <ench/kernel.h>
#include <ench/osstrace.h>

#define REG_N   10
#define OP_MAX  128
#define LB_MAX  32
#define STK_MAX 128
#define SRC_MAX 2048
#define STR_N   16
#define STR_L   48
#define STEP_MAX 500000

enum {
    A_MOV, A_ADD, A_SUB, A_MUL, A_DIV, A_INC, A_DEC,
    A_CMP, A_JMP, A_JE, A_JNE, A_JL, A_JGE,
    A_OUT, A_OUTC, A_PRINT, A_PUSH, A_POP, A_EXIT
};

static const char *const op_names[] = {
    "mov", "add", "sub", "mul", "div", "inc", "dec",
    "cmp", "jmp", "je", "jne", "jl", "jge",
    "out", "outc", "print", "push", "pop", "exit"
};

struct aop { u8 op; u8 a, b; i64 imm; i64 aux; };

static struct aop ops[OP_MAX];
static int nops;
static struct { char name[16]; int at; } labels[LB_MAX];
static int nlabels;
static char jnames[OP_MAX][16];   // цель перехода для каждого jump-op
static char pstrs[STR_N][STR_L];  // строки print
static int npstrs;
static i64 aregs[REG_N];
static i64 astack[STK_MAX];
static int astp;

static char last_src[SRC_MAX];
static int have_last;

// ---- вывод как вывод приложения (J4) ----
static void aputs(const char *s) { console_app_write(s, ench_strlen(s)); }

static void aout_i64(i64 v) {
    char b[26];
    usize n = 0;
    u64 u = v < 0 ? (u64)(-(v + 1)) + 1 : (u64)v;
    if (!u) b[n++] = '0';
    while (u) { b[n++] = (char)('0' + u % 10); u /= 10; }
    if (v < 0) b[n++] = '-';
    char o[26];
    for (usize i = 0; i < n; i++) o[i] = b[n - 1 - i];
    o[n++] = '\n';
    console_app_write(o, n);
}

// ---- лексика ----
static int reg_of(const char *t, int *out) {
    if (t[0] != 'r' || t[1] < '0' || t[1] > '9' || t[2]) return 0;
    *out = t[1] - '0';
    return *out < REG_N;
}

static int imm_of(const char *t, i64 *out) {
    int neg = 0;
    if (*t == '-') { neg = 1; t++; }
    else if (*t == '+') t++;
    if (!*t) return 0;
    u64 v = 0;
    if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        t += 2;
        if (!*t) return 0;
        for (; *t; t++) {
            char c = *t;
            u64 d;
            if (c >= '0' && c <= '9') d = (u64)(c - '0');
            else if (c >= 'a' && c <= 'f') d = (u64)(10 + c - 'a');
            else if (c >= 'A' && c <= 'F') d = (u64)(10 + c - 'A');
            else return 0;
            v = v * 16 + d;
        }
    } else {
        for (; *t; t++) {
            if (*t < '0' || *t > '9') return 0;
            v = v * 10 + (u64)(*t - '0');
        }
    }
    *out = neg ? -(i64)v : (i64)v;
    return 1;
}

// второй операнд: регистр или число
static int operand(const char *t, i64 *val, int *isreg, int *reg) {
    if (reg_of(t, reg)) { *isreg = 1; *val = 0; return 1; }
    if (imm_of(t, val)) { *isreg = 0; *reg = 0; return 1; }
    return 0;
}

// разбить строку на до 3 токенов (запятые = разделители), обрубить комментарий
static int split_tokens(char *s, char tok[3][24]) {
    int nt = 0;
    for (char *p = s; *p; p++) {
        if (*p == ';' || *p == '#') break;
        if (*p == ' ' || *p == '\t' || *p == ',') continue;
        usize l = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != ';' && *p != '#') {
            if (l + 1 < 24) tok[nt][l++] = *p;
            p++;
        }
        tok[nt][l] = 0;
        nt++;
        p--;
        if (nt == 3) break;
    }
    return nt;
}

// строка print: всё после "print" (в кавычках или без), до комментария
static usize take_string(char *s, char *dst, usize cap) {
    while (*s == ' ' || *s == '\t') s++;
    usize l = 0;
    int q = 0;
    if (*s == '"') { q = 1; s++; }
    for (; *s; s++) {
        if (*s == ';' || *s == '#') break;
        if (q && *s == '"') { q = 0; break; }
        if (!q && *s == '\n') break;
        if (l + 1 < cap) dst[l++] = *s;
    }
    dst[l] = 0;
    return l;
}

static int mnemonic_of(const char *t) {
    for (int i = 0; i < (int)ENCH_ARRAY_SIZE(op_names); i++)
        if (ench_strcmp(t, op_names[i]) == 0) return i;
    return -1;
}

static int is_word(const char *s, const char *w) {
    usize l = ench_strlen(w);
    return ench_strncmp(s, w, l) == 0 &&
           (s[l] == ' ' || s[l] == '\t' || s[l] == '"' || s[l] == ';' ||
            s[l] == '#' || s[l] == '\n' || s[l] == 0);
}

// разбор одной строки; 0 = ok/skip, <0 = ошибка
static int parse_line(char *s, int lineno) {
    while (*s == ' ' || *s == '\t') s++;
    if (!*s || *s == '\n' || *s == ';' || *s == '#') return 0;

    // print обрабатываем до токенизации (там строка с пробелами)
    if (is_word(s, "print")) {
        if (nops >= OP_MAX) return -9;
        if (npstrs >= STR_N) return -12;
        take_string(s + 5, pstrs[npstrs], STR_L);
        ops[nops].op = A_PRINT;
        ops[nops].a = ops[nops].b = 0;
        ops[nops].imm = 0;
        ops[nops].aux = npstrs++;
        nops++;
        (void)lineno;
        return 0;
    }

    char tok[3][24];
    int nt = split_tokens(s, tok);
    if (!nt) return 0;

    // метка "name:"
    usize tl = ench_strlen(tok[0]);
    if (tl && tok[0][tl - 1] == ':') {
        if (nt > 1) return -7; // метка должна быть одна на строке
        tok[0][tl - 1] = 0;
        if (nlabels >= LB_MAX) return -8;
        usize l = ench_strlen(tok[0]);
        if (l >= sizeof labels[0].name) l = sizeof labels[0].name - 1;
        ench_memcpy(labels[nlabels].name, tok[0], l);
        labels[nlabels].name[l] = 0;
        labels[nlabels].at = nops;
        nlabels++;
        (void)lineno;
        return 0;
    }

    if (nops >= OP_MAX) return -9;
    int mn = mnemonic_of(tok[0]);
    if (mn < 0) return -10;

    struct aop *o = &ops[nops];
    o->a = o->b = 0;
    o->imm = 0;
    o->aux = -1;

    int r1, r2, isr2;
    i64 v2;

    switch (mn) {
    case A_MOV: case A_ADD: case A_SUB: case A_MUL: case A_DIV: case A_CMP: {
        if (nt < 3 || !reg_of(tok[1], &r1) || !operand(tok[2], &v2, &isr2, &r2))
            return -11;
        o->op = (u8)mn;
        o->a = (u8)r1;
        o->b = (u8)(isr2 ? r2 : 0);
        o->imm = v2;
        o->aux = isr2;
        nops++;
        return 0;
    }
    case A_INC: case A_DEC: case A_OUT: case A_OUTC: case A_PUSH: case A_POP: {
        if (nt < 2 || !reg_of(tok[1], &r1)) return -11;
        o->op = (u8)mn;
        o->a = (u8)r1;
        nops++;
        return 0;
    }
    case A_JMP: case A_JE: case A_JNE: case A_JL: case A_JGE: {
        if (nt < 2 || !tok[1][0]) return -11;
        usize l = ench_strlen(tok[1]);
        if (l >= sizeof jnames[0]) l = sizeof jnames[0] - 1;
        ench_memcpy(jnames[nops], tok[1], l);
        jnames[nops][l] = 0;
        o->op = (u8)mn;
        o->imm = -1; // резолв на pass2
        nops++;
        return 0;
    }
    case A_EXIT: {
        if (nt > 1) return -11;
        o->op = A_EXIT;
        nops++;
        return 0;
    }
    }
    return -10;
}

// ---- исполнение ----
static int run(i64 *retval) {
    // pass2: резолв меток переходов
    for (int i = 0; i < nops; i++) {
        if (ops[i].op < A_JMP || ops[i].op > A_JGE) continue;
        int found = -1;
        for (int j = 0; j < nlabels; j++)
            if (ench_strcmp(labels[j].name, jnames[i]) == 0) { found = labels[j].at; break; }
        if (found < 0) return ENCH_EINVAL;
        ops[i].imm = found;
    }

    for (int i = 0; i < REG_N; i++) aregs[i] = 0;
    astp = 0;
    int cmp_lt = 0, cmp_eq = 0;

    int steps = 0;
    for (int pc = 0; pc >= 0 && pc < nops; steps++) {
        if (steps > STEP_MAX) return ENCH_EUNKNOWN; // зациклилась
        struct aop *o = &ops[pc];
        i64 *ra = &aregs[o->a];
        i64 rb = o->aux == 1 ? aregs[o->b] : o->imm;
        int next = pc + 1;
        switch (o->op) {
        case A_MOV: *ra = rb; break;
        case A_ADD: *ra += rb; break;
        case A_SUB: *ra -= rb; break;
        case A_MUL: *ra *= rb; break;
        case A_DIV:
            if (rb == 0) return ENCH_EINVAL;
            *ra /= rb;
            break;
        case A_INC: (*ra)++; break;
        case A_DEC: (*ra)--; break;
        case A_CMP:
            cmp_lt = *ra < rb;
            cmp_eq = *ra == rb;
            break;
        case A_JMP: next = (int)o->imm; break;
        case A_JE:  next = cmp_eq ? (int)o->imm : pc + 1; break;
        case A_JNE: next = !cmp_eq ? (int)o->imm : pc + 1; break;
        case A_JL:  next = cmp_lt ? (int)o->imm : pc + 1; break;
        case A_JGE: next = !cmp_lt ? (int)o->imm : pc + 1; break;
        case A_OUT: aout_i64(*ra); break;
        case A_OUTC: {
            char c = (char)*ra;
            console_app_write(&c, 1);
            break;
        }
        case A_PRINT: aputs(pstrs[o->aux]); break;
        case A_PUSH:
            if (astp >= STK_MAX) return ENCH_ENOSPC;
            astack[astp++] = *ra;
            break;
        case A_POP:
            if (astp <= 0) return ENCH_EINVAL;
            *ra = astack[--astp];
            break;
        case A_EXIT:
            if (retval) *retval = aregs[0];
            return ENCH_EOK;
        default:
            return ENCH_EINVAL;
        }
        pc = next;
    }
    if (retval) *retval = aregs[0];
    return ENCH_EOK; // конец программы без exit
}

// ---- публичный API ----
static int ends_with_easm(const char *name) {
    usize l = ench_strlen(name);
    return l > 5 && ench_strcmp(name + l - 5, ".easm") == 0;
}

i32 asm_run_file(const char *path, i64 *retval) {
    if (!path) return ENCH_EINVAL;
    i32 fd = fs_open(path, ENCH_O_READ);
    if (fd < 0) return fd;
    i64 n = fs_read(fd, last_src, SRC_MAX - 1);
    fs_close(fd);
    if (n < 0) return ENCH_EIO;
    last_src[n] = 0;
    have_last = 1;

    // разбор
    nops = nlabels = npstrs = 0;
    int lineno = 0;
    char *p = last_src;
    while (p) {
        lineno++;
        char *nl = p;
        while (*nl && *nl != '\n') nl++;
        int ended = (*nl == 0);
        *nl = 0;
        int r = parse_line(p, lineno);
        if (r < 0) {
            kprintf("asm: %s:%d: ошибка разбора (%d)\n", path, lineno, r);
            nops = 0;
            return ENCH_EINVAL;
        }
        if (ended) break;
        p = nl + 1;
    }
    if (!nops) {
        kprintf("asm: %s: нет команд\n", path);
        return ENCH_EINVAL;
    }

    OssTrace_AsmDump(path, last_src, (usize)n);
    i32 r = run(retval);
    if (r != ENCH_EOK)
        kprintf("asm: %s: ошибка исполнения (%d)\n", path, r);
    return r;
}

i32 asm_list(char *out, usize cap) {
    if (!out || !cap) return ENCH_EINVAL;
    char buf[1024];
    i32 n = fs_listdir(ENCH_EASM_DIR, buf, sizeof buf, 0);
    if (n < 0) return n;
    usize o = 0;
    i32 cnt = 0;
    usize i = 0;
    while (i < sizeof buf && buf[i]) {
        char name[64];
        usize l = 0;
        while (buf[i] && buf[i] != '\n' && l + 1 < sizeof name) name[l++] = buf[i++];
        if (buf[i] == '\n') i++;
        name[l] = 0;
        if (!l || !ends_with_easm(name)) continue;
        for (usize j = 0; name[j] && o + 1 < cap; j++) out[o++] = name[j];
        if (o + 1 < cap) out[o++] = '\n';
        cnt++;
    }
    if (o < cap) out[o] = 0;
    return cnt;
}

// OssTrace_AsmDump: разбор последней программы
void asm_dump_last(void) {
    if (!have_last) { aputs("asm: программ ещё не было\n"); return; }
    aputs("--- OssTrace_AsmDump ---\n");
    aputs(last_src);
    aputs("--- ops ---\n");
    for (int i = 0; i < nops; i++) {
        char line[96];
        usize o = 0;
        const char *nm = ops[i].op < ENCH_ARRAY_SIZE(op_names) ? op_names[ops[i].op] : "?";
        for (usize j = 0; nm[j] && o < 90; j++) line[o++] = nm[j];
        if (o < 90) line[o++] = ' ';
        if (o < 90) line[o++] = 'r';
        if (o < 90) line[o++] = (char)('0' + ops[i].a);
        if (ops[i].op == A_PRINT) {
            if (o < 90) line[o++] = ' ';
            if (o < 90) line[o++] = '"';
            for (usize j = 0; pstrs[ops[i].aux][j] && o < 88; j++) line[o++] = pstrs[ops[i].aux][j];
            if (o < 90) line[o++] = '"';
        } else if (ops[i].op >= A_JMP && ops[i].op <= A_JGE) {
            if (o < 90) line[o++] = ' ';
            for (usize j = 0; jnames[i][j] && o < 88; j++) line[o++] = jnames[i][j];
        } else if (ops[i].op != A_EXIT) {
            if (o < 90) line[o++] = ',';
            if (ops[i].aux == 1) { // регистр
                if (o < 90) line[o++] = 'r';
                if (o < 90) line[o++] = (char)('0' + ops[i].b);
            } else {               // число
                i64 v = ops[i].imm;
                char num[24];
                usize nn = 0;
                u64 u = v < 0 ? (u64)(-(v + 1)) + 1 : (u64)v;
                if (!u) num[nn++] = '0';
                while (u) { num[nn++] = (char)('0' + u % 10); u /= 10; }
                if (v < 0 && o < 90) line[o++] = '-';
                while (nn && o < 90) line[o++] = num[--nn];
            }
        }
        if (o < 90) line[o++] = '\n';
        console_app_write(line, o);
    }
    aputs("--- конец дампа ---\n");
}
