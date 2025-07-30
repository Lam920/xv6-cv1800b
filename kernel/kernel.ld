/* Physical memory layout
 *
 * qemu -machine virt is set up like this,
 * based on qemu's hw/riscv/virt.c:
 *
 * 00001000 -- boot ROM, provided by qemu
 * 02000000 -- CLINT
 * 0C000000 -- PLIC
 * 10000000 -- uart0
 * 10001000 -- virtio disk
 * KERNBASE -- boot ROM/OpenSBI jumps here in M-mode/S-mode
 *             -kernel loads the kernel here
 *
 * the kernel uses physical memory thus:
 * KERNBASE -- entry.S, then kernel text and data
 * end -- start of kernel page allocation area
 * PHYSTOP -- end RAM used by the kernel
 */
/* Copyright (C) 2023 Jisheng Zhang <jszhang@kernel.org> */
/* #define GPIO0 0x3020000 */
/* #define GPIO1 0x3021000 */
/* #define GPIO3 0x3023000 */
/* #define PWM0 0x3060000 */
/* #define PWM2 0x3062000 */
/* #define PWM3 0x3063000 */
/* core local interruptor (CLINT), which contains the timer. */
/* qemu puts platform-level interrupt controller (PLIC) here. */
/* the kernel expects there to be RAM
 * for use by the kernel and user pages
 * from physical address KERNBASE to PHYSTOP.
 */
/* map the trampoline page to the highest address,
 * in both user and kernel space.
 */
/* map kernel stacks beneath the trampoline,
 * each surrounded by invalid guard pages.
 */
/* User memory layout.
 * Address zero first:
 *   text
 *   original data and bss
 *   fixed-size stack
 *   expandable heap
 *   ...
 *   TRAPFRAME (p->trapframe, used by the trampoline)
 *   TRAMPOLINE (the same page as in the kernel)
 */
OUTPUT_ARCH( "riscv" )
ENTRY( _entry )
SECTIONS
{
  /*
   * ensure that entry.S / _entry is at KERNBASE,
   * where qemu's -kernel jumps.
   */
  . = (0x80200000);
  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(etext = .);
  }
  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
    . = ALIGN(0x1000);
    __ramdisk_start = .;
    *(.ramdisk .ramdisk.*)
    __ramdisk_end = .;
  }
  .bss : {
    . = ALIGN(16);
    _bss_start = .;
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    *(.bss .bss.*)
    . = ALIGN(16);
    _bss_end = .;
  }
  . = ALIGN(0x1000);
  PROVIDE(_end = .);
}
