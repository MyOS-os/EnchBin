// kernel/modules/drive/src/drive_mod.c — модуль ядра drive.mke.
// Собирается reloc-ELF (ld -r) и встраивается в ядро через секцию .enchmke
// (builder: objcopy --rename-section .data=.enchmke); при загрузке ядра
// drive/mke.c находит экспорт ench_mke_init (ENCH_MKE_SYMBOL) и вызывает его.
// Роль модуля — точка активации подсистемы драйверов: реестр и загрузчик
// .b_d живут в ядре (drive/drive.c через ksyms); автозагрузка .b_d —
// init.d_/main.einit (`load /Platform/ench.bin/system/drivers/*.b_d`).
#include <ench/kernel.h>
#include <ench/osstrace.h>

void ench_mke_init(void) {
    kprintf("drive.mke: подсистема драйверов активна\n");
    OssTrace_Trace("mke drive: init");
}
