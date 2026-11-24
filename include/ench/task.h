/* include/ench/task.h — задачи и планировщик (вес/rt).
 * Реализация: kernel/src/task/task.c; объявления — в ench/kernel.h (task_*),
 * состояния ENCH_TASK_* и struct ench_taskinfo — в ench/abi.h.
 * Планировщик: weighted round-robin — вес по умолчанию = размер ELF-образа/1024,
 * клампа [1,1024] (task_weight_for_size); SYS_setweight перекрывает.
 * Квант = базовый срез * вес. Real-time поток (SYS_setrt): не вытесняется
 * по тику таймера — уступает CPU только сам (yield/block/exit). */
#ifndef ENCH_TASK_H
#define ENCH_TASK_H

#define ENCH_TASK_MAX     64
#define ENCH_WEIGHT_MAX   1024
#define ENCH_FD_MAX       16

#endif
