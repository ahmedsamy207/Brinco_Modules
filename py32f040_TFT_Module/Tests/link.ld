/* Software tests only: Python invokes functions directly without MCU startup. */
ENTRY(Test_Init)
MEMORY {
  FLASH (rx) : ORIGIN = 0x00010000, LENGTH = 128K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
}
SECTIONS {
  .text : { *(.text*) *(.rodata*) } > FLASH
  .data : { _sdata = .; *(.data*) _edata = .; } > RAM AT > FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
}
