/* include/ench/osstrace.h — OssTrace: диагностические переключатели stdlib.
 * Значения 1/0 задаются при компиляции единицы (build.json cflags/builder):
 *   -D EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace=1   trace ОС
 *   -D OssTrace_AsmDump=1                                       дамп регистров
 *   -D OssTrace_FileLog=1                                       журнал на диск
 * Единственная реализация: stdlib/c/src/osstrace.c (собирается и в ядро
 * с -DENCH_KERNEL, и в libench для ring3). Журнал:
 *   /Platform/data/system/log/osstrace.log
 */
#ifndef ENCH_OSSTRACE_H
#define ENCH_OSSTRACE_H
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

#if EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace
  #define OSSTRACE(...)      oss_trace(__VA_ARGS__)
  #define OSSTRACE_ENTER(nm) oss_trace("-> %s", (nm))
  #define OSSTRACE_LEAVE(nm) oss_trace("<- %s", (nm))
#else
  #define OSSTRACE(...)      ((void)0)
  #define OSSTRACE_ENTER(nm) ((void)0)
  #define OSSTRACE_LEAVE(nm) ((void)0)
#endif

#if OssTrace_FileLog
  #define OSSFILELOG(...)    oss_filelogf(__VA_ARGS__)
#else
  #define OSSFILELOG(...)    ((void)0)
#endif

#if OssTrace_AsmDump
  #define OSSASMDUMP()       oss_asmdump()
#else
  #define OSSASMDUMP()       ((void)0)
#endif

void oss_trace(const char *fmt, ...);
void oss_filelogf(const char *fmt, ...);
void oss_asmdump(void); /* дамп rax..r15/rflags/rsp (asm-thunk) */

#endif
