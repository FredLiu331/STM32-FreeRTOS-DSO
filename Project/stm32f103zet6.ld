/* STM32F103ZET6 Linker Script */
ENTRY(Reset_Handler)

/* 内存分区定义 */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 64K
}

/* 栈顶位置：RAM末尾 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  /* 中断向量表 */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  /* 代码段 */
  .text :
  {
    . = ALIGN(4);
    *(.text)           
    *(.text*)          
    *(.glue_7)         
    *(.glue_7t)        
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;        
  } > FLASH

  /* 只读数据 */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  /* 用于初始化的数据段信息 */
  _sidata = LOADADDR(.data);

  /* 已初始化变量段 (运行时位于 RAM, 初始值存放在 FLASH) */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;        
  } > RAM AT> FLASH

  /* 未初始化变量段 */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;         
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         
    __bss_end__ = _ebss;
  } > RAM

  /* 堆栈空间检查 */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + 0x200;    /* Min Heap Size */
    . = . + 0x400;    /* Min Stack Size */
    . = ALIGN(8);
  } > RAM

  /* 移除标准库不需要的段 */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }
}