/* scripts/link/app.ld — приложения ring 3 (User/apps) */
ENTRY(_start)

SECTIONS {
  . = 0x400000;

  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(COMMON) *(.bss*) }
}
