#pragma once
/* include/ench/account.h — аккаунт-система (документация).
 * Реализация: kernel/src/account/account.c; объявления — в ench/kernel.h
 * (acc_count/acc_create/acc_login/acc_list/acc_current/acc_set_current).
 *
 * Хранение: ENCH_ACC_DIR ("/Platform/data/system/.data-hash-user")/
 *   <логин>/ENCH_PASSFILE (".pass.__")
 *   - суффикс ".__" => файл скрытый и lock: cat -> err, SYS_read -> ELOCKED;
 *     ядро читает его через fs_raw_read (без lock-проверок)
 * Хеш (игрушечный, до salted-SHA-256 из roadmap): FNV-1a 64,
 * соль = логин + ':' + пароль; хранится как 16 hex-символов + '\n'.
 * Первый запуск: acc_count()==0 => shell предлагает создание логина+пароля;
 * после reboot — только вход. Логин: [A-Za-z0-9_-], 1..24 символов. */
