// include/ench/osstrace.h — OssTrace: трассировка 1/0 на этапе компиляции (J5).
// Управление:
//   cc -DEnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace=1
//   cc -DEnchBin_RingThree_Debug_FunctionDebug_OssTrace_AsmDump=1
//   cc -DEnchBin_RingThree_Debug_FunctionDebug_OssTrace_FileLog=1
// По умолчанию всё выключено (0). Реализация в stdlib/c/src/osstrace.c:
// она же собирается и в ядро (ENCH_KERNEL), и в libench.
#pragma once

#ifndef EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace
#define EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace 0
#endif

#ifndef EnchBin_RingThree_Debug_FunctionDebug_OssTrace_AsmDump
#define EnchBin_RingThree_Debug_FunctionDebug_OssTrace_AsmDump 0
#endif

#ifndef EnchBin_RingThree_Debug_FunctionDebug_OssTrace_FileLog
#define EnchBin_RingThree_Debug_FunctionDebug_OssTrace_FileLog 0
#endif

#ifdef ENCH_KERNEL
// kernel: Trace -> COM1+экран (если консоль свободна), FileLog -> ENCH_LOG_DIR/kernel.log
void ench_trace_msg(const char *fmt, ...);
void ench_filelog(const char *fmt, ...);
#else
// ring3: всё через write() в файл лога /Platform/data/system/log/<pid>.log
void ench_trace_msg(const char *fmt, ...);
void ench_filelog(const char *fmt, ...);
#endif

// дамп машинного кода динамического ассемблера (kernel и ring3-клиент)
void ench_asmdump(const char *tag, const void *code, usize len);

#define OssTrace_Trace(...)      do { if (EnchBin_RingThree_Debug_FunctionDebug_OssTrace_Trace) ench_trace_msg(__VA_ARGS__); } while (0)
#define OssTrace_FileLog(...)    do { if (EnchBin_RingThree_Debug_FunctionDebug_OssTrace_FileLog) ench_filelog(__VA_ARGS__); } while (0)
#define OssTrace_AsmDump(tag, code, len) do { if (EnchBin_RingThree_Debug_FunctionDebug_OssTrace_AsmDump) ench_asmdump((tag), (code), (len)); } while (0)
