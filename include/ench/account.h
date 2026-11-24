#pragma once
/* include/ench/account.h — аккаунт-система.
 * Хранение: /Platform/data/system/.data-hash-user/<логин>/.pass.__
 *   - '__'-суффикс => F_HIDDEN: не видно без ls --a
 *   - файл помечен F_SECURE: cat -> err, SYS_read -> EACCES
 * Хеш (игрушечный, до SHA-256/salted из roadmap): FNV-1a 64 с логином-солью.
 * Первый запуск: аккаунтов нет => shell предлагает создание логина+пароля.
 * После reboot: только вход. Неверный пароль => err и заново. */
#include <stdint.h>

#define ACC_DIR "/Platform/data/system/.data-hash-user"

void        acc_init(void);
int         acc_create(const char *login, const char *pass);
int         acc_login(const char *login, const char *pass);
void        acc_logout(void);
int         acc_list(struct ench_acc_list *out);
const char *acc_current(void);       /* NULL если не вошли */
int         acc_logged_in(void);
int         acc_firstboot(void);     /* нет ни одного аккаунта */
int         acc_is_hidden_name(const char *name);

/* проверка пары логин/.pass.__ без входа (для cat-протокола) */
int         acc_check_file(const struct fs_node *n);
