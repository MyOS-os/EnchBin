/* scripts/link/kernel.ld — ядро EnchBin (limine, higher half) */
ENTRY(ench_entry)

SECTIONS {
  . = 0xFFFFFFFF80000000;

  .limine_requests : {
    KEEP(*(.limine_requests))
    KEEP(*(.limine_requests_start))
    KEEP(*(.limine_requests_end))
  }

  .text : { *(.text*) }
  .enchmke : ALIGN(16) { *(.enchmke) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(COMMON) *(.bss*) }

  _ench_kernel_end = .;
}
