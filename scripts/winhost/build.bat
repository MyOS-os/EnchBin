@echo off
rem EnchBin: сборка на Windows-хосте. Вход — builder.c, не make напрямую.
where cc >nul 2>nul
if errorlevel 1 (
  echo need a POSIX cc ^(mingw/wsl^) in PATH
  exit /b 1
)
if not exist bin mkdir bin
cc -O2 -o bin\builder.exe builder.c || exit /b 1
bin\builder.exe %*
