# Roadmap EnchBin OS v0.1.0 «First Light»

## Рефакторинг структуры
- [x] core/* перенесены из глубины в корень-уровень модулей (kernel/, drivers/, userspace/)
- [x] limine -> third_party/limine (скачивается builder'ом)
- [x] все .h из корня и из исходников -> include/ (глобально) и <папка>/include/ (локально)
- [x] make/cmake и т.д. -> scripts/ ; их вызывает builder.c (корень)
- [x] внутри каждой папки: src/, include/, scripts/ — где надо, упаковано по папкам
- [x] удалены все demo/hello примеры, dmod, .mk-модули и дубликаты
- [x] репозиторий содержит только исходники (без *.o, *.elf, *.iso, *.b_d, *.mke)

## Ядро
- [x] boot: Limine protocol, HHDM, framebuffer, memmap, модуль-тарбол диска
- [x] консоль на framebuffer (шрифт 3x5, ANSI CSI, цвета, прокрутка)
- [x] PMM/VMM (4-уровневый пейджинг), heap (kmalloc/kfree)
- [x] GDT/IDT/TSS, PIT для uptime
- [x] ramfs + tar-загрузка скелета диска
- [x] планировщик задач: weighted round-robin, вес задачи = вес кода (SYS_setweight)
- [x] real-time поток без прерываний (SYS_setrt): всегда получает время
- [x] syscalls 0..26 (ring 3)
- [x] ELF64 загрузчик пользовательских приложений
- [x] загрузчик reloc-ELF (.mke/.b_d/appscore) с RELATIVE/PC32 релокациями

## Драйверы и модули
- [x] формат модулей ядра .mke (не .mk); единственный модуль — drive.mke
- [x] drive.mke: реестр драйверов, загрузка .b_d в drive-область памяти
- [x] драйверы .b_d: keyboard, ansi, unix_lin (Linux-совместимость syscall)
- [x] msdos драйвер: исходники в userspace/driversuser/msdos -> msdos.b_d
- [x] все драйверы загружаются init.d_ и передаются в drive-область памяти
- [x] init.d_/autostart: автозапуск (shell)
- [ ] раскладка ru для клавиатуры
- [ ] SHA-256/salted-хеш вместо ench-hash64
- [ ] ext2/чтение реального диска
- [ ] сеть, SMP

## Shell (ring 3)
- [x] удалены все $-команды (включая $cd)
- [x] run -> :: ; runprog сохранён
- [x] cd --path="/полный/путь" или cd имя_папки (на один уровень)
- [x] ls показывает только текущий каталог; ls --a — со скрытыми
- [x] cat файла с __ или . даёт err
- [x] протокол вывода: только вывод программы, последняя строка — код завершения (с новой строки)
- [x] единый стиль аргументов: --opt, --opt="значение", -a, /пути
- [x] команды: tasks, drivers, users, passwd, useradd, mem, procs и т.д.

## Аккаунты
- [x] первый запуск: создание логина+пароля
- [x] хеш в Platform/data/system/.data-hash-user/<логин>/.pass.__
- [x] скрытие без ls --a; __ = lock; cat пароля -> err
- [x] после reboot — только вход; неверный пароль -> err и заново

## Приложения
- [x] appscore (ring 0): sysinfo, fsprobe, memmap, devctl -> Platform/Apps
- [x] appsuser (ring 3): shell, edit, calc, clock -> User/apps
- [x] runasm: динамический исполнитель .easm из /Platform/data/asm
- [x] GUI-замашки: Platform/MyOS (gui — окна/кнопки/фокус, gfx — примитивы)
- [ ] мышь и полноценный оконный менеджер
- [ ] шрифт с кириллицей

## Stdlib
- [x] C: libench (syscall, console, fs, str) + crt0
- [x] Rust: крейт ench (no_std) — syscall, console, fs, osstrace
- [x] D: dub-библиотека ench — syscall, console, fs, osstrace
- [x] asm: syscall-транк, dumpregs
- [x] OssTrace: EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace, OssTrace_AsmDump, OssTrace_FileLog (1/0, define при компиляции)
- [x] filelog: /Platform/data/system/log/osstrace.log
