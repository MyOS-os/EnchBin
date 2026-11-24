#pragma once
/* include/ench/limine.h — обёртка над протокольным заголовком limine.
 * Настоящий limine.h скачивается builder'ом (LIMINE_VERSION) в
 * third_party/limine/limine.h и подключается через -I third_party/limine.
 * Репозиторий содержит только исходники — бинарные/сторонние данные качаются при сборке. */
#if __has_include(<limine.h>)
  #include <limine.h>
#else
  #error "нет third_party/limine/limine.h: запусти 'builder build' (или fetch) — builder скачает limine"
#endif
