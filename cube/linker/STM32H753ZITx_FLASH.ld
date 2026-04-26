/*
 * STM32H753ZI — flash @ 0x08000000, DTCM for stack/heap.
 * Based on ST CMSIS device template (GNU ld).
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);

_Min_Heap_Size = 0x200;
/* Quartet + printf: default 0x400 was tight; avoid silent stack smash into heap. */
_Min_Stack_Size = 0x2000;

MEMORY
{
  FLASH (rx)     : ORIGIN = 0x08000000, LENGTH = 2048K
  DTCMRAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 128K
  RAM_D1 (xrw)   : ORIGIN = 0x24000000, LENGTH = 512K
  RAM_D2 (xrw)   : ORIGIN = 0x30000000, LENGTH = 288K
  RAM_D3 (xrw)   : ORIGIN = 0x38000000, LENGTH = 64K
  ITCMRAM (xrw)  : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
    KEEP (*(.init))
    KEEP (*(.fini))
    _etext = .;
  } > FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM_D1 AT> FLASH

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
    PROVIDE(end = .);
  } > RAM_D1

  ._user_heap_stack :
  {
    . = ALIGN(8);
    _end = .;
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > DTCMRAM
}
