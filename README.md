# EnchBin OS v0.1.0 «First Light»

Игровая (toy) операционная система для **x86_64**: freestanding C11, загрузка по
**Limine boot protocol**. В репозитории — **только исходники**, без скомпилированных
частей, без demo/hello, без dmod и любых дубликатов.

## Структура репозитория

Все `.h` собраны в `include/` (глобальные) и в `include/` конкретных папок.
Все make/cmake-файлы — в `scripts/`, их вызывает `builder.c` (единая точка сборки в корне).
Внутри каждой папки — тот же принцип: `src/` — код, `include/` — заголовки,
`scripts/` — сборочная мелочь, если нужна.

```
EnchBin/
├── builder.c                  # единая точка сборки (корень)
├── scripts/                   # make/cmake и т.д. — вызываются builder'ом
│   ├── build.json             # манифест сборки
│   ├── Makefile               # команды cc/ld/ar
│   ├── cmake/CMakeLists.txt   # обёртка над builder
│   ├── winhost/               # сборка на Windows-хосте
│   ├── link/kernel.ld         # скрипт линковки ядра
│   ├── link/app.ld            # скрипт линковки приложений ring3
│   └── limine.conf.in         # конфиг загрузчика
├── include/ench/              # ВСЕ глобальные .h (ядро, ABI, stdlib, OssTrace)
├── kernel/                    # ядро
│   ├── src/                   # boot, console, mm, task, syscall, fs, account, drive, drv
│   └── modules/drive/         # единственный модуль ядра -> drive.mke
├── drivers/                   # драйверы .b_d: keyboard, ansi, unix_lin
├── userspace/
│   ├── appscore/              # ring 0 приложения -> Platform/Apps (мой GUI: Platform/MyOS)
│   │   └── myos/              # замашки под GUI
│   ├── appsuser/              # ring 3 приложения -> User/apps (shell, edit, calc, clock, runasm)
│   └── driversuser/           # исходники драйверов уровня пользователей (msdos)
├── stdlib/
│   ├── c/                     # libench: syscall-обёртки, console, fs, osstrace
│   ├── rust/                  # крейт ench (no_std)
│   ├── d/                     # библиотека ench (dub)
│   └── asm/                   # syscall.thunk + dumpregs (OssTrace_AsmDump)
├── disk/                      # скелет диска (structure): Platform/, User/
├── docs/                      # architecture, shell, drivers, accounts, osstrace, sched, gui, runasm
└── third_party/limine/        # limine (скачивается builder'ом при сборке)
```

## Структура диска (собирается builder'ом в образ)

```
Platform/
├── Apps/                      # ring 0 приложения (sysinfo, fsprobe, memmap, devctl)
├── MyOS/                      # GUI-прототипы (gui, gfx) — разделение ядра и пользовательского кода
├── data/system/
│   ├── init.d_/               # autostart.einit, drivers.einit
│   ├── .data-hash-user/       # <логин>/.pass.__ (скрытые: суффикс __ = lock)
│   ├── log/                   # osstrace.log (filelog)
│   └── asm/                   # *.easm — динамический исполнитель ассемблера
└── ench.bin/system/
    ├── drivers/               # keyboard.b_d, ansi.b_d
    │   └── userd/             # msdos.b_d
    └── drive/                 # Unix_lin.b_d (+ прочие .b_d)
User/
└── apps/                      # ring 3 приложения (shell, edit, calc, clock, runasm)
```

## Сборка

```sh
cc -O2 -o bin/builder builder.c
bin/builder build          # ядро, drive.mke, .b_d, приложения, образ ISO
bin/builder run            # сборка + qemu-system-x86_64
bin/builder clean
```

`builder.c` читает `scripts/build.json` и вызывает `scripts/Makefile` (или cmake-обёртку).
Limine скачивается в `third_party/limine/` при сборке (можно положить вручную).

## Оболочка (ring 3)

- `$`-команд нет; запуск программы — `::` или `runprog`
- `cd --path="/полный/путь"` либо `cd имя_папки` (на один уровень)
- `ls` показывает только текущий каталог; `ls --a` — со скрытыми (`.` и `__`)
- `cat` файла с суффиксом `__` или `.`-файла даёт `err`
- при запуске программы выводится только вывод программы, последней строкой —
  код завершения, всегда с новой строки

## OssTrace (stdlib, включается при сборке приложения)

```sh
-D EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace=1   # trace ОС: 1 да, 0 нет
-D OssTrace_AsmDump=1                                       # дамп регистров
-D OssTrace_FileLog=1                                       # журнал в /Platform/data/system/log/osstrace.log
```

## Планировщик и потоки

- weighted round-robin: вес задачи = «вес кода» (размер ELF), задаётся/меняется `SYS_setweight`
- real-time поток (`SYS_setrt`, `:: clock --rt`): без прерываний, планировщик всегда
  выдаёт время этому потоку, пока он сам не заблокируется

## Аккаунты

Первый запуск: создание логина и пароля. Хеш —
`Platform/data/system/.data-hash-user/<логин>/.pass.__`. После reboot — только вход;
неверный пароль — `err` и заново.

## Документация

`docs/architecture.md`, `docs/shell.md`, `docs/drivers.md`, `docs/accounts.md`,
`docs/osstrace.md`, `docs/sched.md`, `docs/gui.md`, `docs/runasm.md`, `roadmap.md`.
