#pragma once
/* include/ench/fs.h — ramfs на ustar-тарболе (модуль limine enchdisk.tar) +
 * write-оверлей в heap. Пути всегда абсолютные от '/'.
 * __-суффикс => F_HIDDEN (ls --a), аккаунт-файлы => F_SECURE (cat -> err). */
#include <stdint.h>
#include <stddef.h>

struct fs_node {
    char     name[64];
    uint32_t flags;            /* F_DIR/F_HIDDEN/F_SECURE */
    uint32_t size;
    const void *data;          /* HHDM-указатель в тарбол (RO) или heap (RW) */
    int      rw;               /* 1 = write-оверлей (heap) */
    struct fs_node *parent, *child, *sibling;
};

void fs_init(void *tar_addr, size_t tar_len); /* парсинг ustar в дерево */
struct fs_node *fs_root(void);

int  fs_lookup(const char *abs_path, struct fs_node **out);
int  fs_read(const struct fs_node *n, uint64_t off, void *buf, size_t len);
int  fs_write(struct fs_node *n, const void *buf, size_t len);        /* append для rw */
int  fs_trunc(struct fs_node *n);
int  fs_create(struct fs_node *dir, const char *name, uint32_t flags, struct fs_node **out);
int  fs_mkdir(struct fs_node *dir, const char *name, struct fs_node **out);
int  fs_unlink(struct fs_node *n);

struct fs_node *fs_child_first(struct fs_node *dir);
struct fs_node *fs_child_next(struct fs_node *n);   /* итератор для ls */
void fs_node_path(const struct fs_node *n, char *out, size_t cap);

uint32_t fs_flags_for_name(const char *name, int secure_hint);
const char *fs_strerror(int err);
