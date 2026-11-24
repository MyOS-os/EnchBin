# winhost — сборка EnchBin на Windows-хосте

Точка входа одна — `builder.c` в корне. `build.bat` компилирует builder и
передаёт ему аргументы (`build`, `run`, `clean`, `kernel`, `drivers`, ...).

Нужен POSIX-совместимый `cc` в PATH (mingw-w64 или cc из WSL), а также
`make`, `ld`, `objcopy`, `xorriso`, `qemu-system-x86_64` при полной сборке образа.

Запуск:

    scripts\winhost\build.bat build
    scripts\winhost\build.bat run
