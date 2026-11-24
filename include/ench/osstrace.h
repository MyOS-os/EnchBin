#pragma once
/* include/ench/osstrace.h — OssTrace: диагностические переключатели компиляции
 * (1 = включено, 0 = выключено; задаётся при сборке приложения):
 *
 *   EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace = 1/0  — trace ОС
 *   OssTrace_AsmDump                                     = 1/0  — дамп регистров/кода
 *   OssTrace_FileLog                                     = 1/0  — журнал в
 *        /Platform/data/system/log/osstrace.log
 *
 * Принадлежит stdlib: единственная реализация stdlib/c/src/osstrace.c,
 * компилируется и в ядро (-DENCH_KERNEL), и в libench (ring3).
 */
#include <stdint.h>
#include <stddef.h>

#ifndef EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace
#define EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace 0
#endif
#ifndef OssTrace_AsmDump
#define OssTrace_AsmDump 0
#endif
#ifndef OssTrace_FileLog
#define OssTrace_FileLog 0
#endif

#define ENCH_OSSTRACE_TRACE EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace

#if ENCH_OSSTRACE_TRACE
  #define OSS_TRACE(tag, ...) oss_trace(tag, __VA_ARGS__)
#else
  #define OSS_TRACE(tag, ...) ((void)0)
#endif

#if OssTrace_AsmDump
  #define OSS_ASMDUMP(code, len, label) oss_asmdump(code, len, label)
#else
  #define OSS_ASMDUMP(code, len, label) ((void)0)
#endif

#if OssTrace_FileLog
  #define OSS_FILELOG(line) oss_filelog(line)
#else
  #define OSS_FILELOG(line) ((void)0)
#endif

void oss_trace(const char *tag, const char *fmt, ...);
void oss_asmdump(const void *code, uint64_t len, const char *label);
void oss_filelog(const char *line);
void oss_filelog_init(void);
