/* include/ench/task.h — задачи, планировщик (вес/rt). Реализация: kernel/src/task.c.
 * Планировщик: weighted round-robin (вес = "вес кода", единица = страница 4K образа;
 * SYS_setweight). Real-time поток (SYS_setrt): планировщик всегда даёт ему время,
 * он не прерывается таймером и уступает CPU только сам (yield/block/exit). */
#ifndef ENCH_TASK_H
#define ENCH_TASK_H
#include <stdint.h>

#define ENCH_TASK_MAX     64
#define ENCH_KSTACK_PAGES 4
#define ENCH_BASE_SLICE   2   /* тика PIT (10 мс каждый) — базовый квант */
#define ENCH_WEIGHT_MAX   1024
#define ENCH_FD_MAX       16

#endif
