/* 物理メモリのレイアウト
 *
 * qemu -machine virt は次のように設定されている。
 *   参照: qemuの hw/riscv/virt.c
 *
 * 00001000 -- boot ROM, provided by qemu
 * 02000000 -- CLINT
 * 0C000000 -- PLIC
 * 10000000 -- uart0
 * 10001000 -- virtio disk
 * KERNBASE -- boot ROM/OpenSBI jumps here in M-mode/S-mode
 *             -kernel loads the kernel here
 *
 */
/* cv1800b のペリフェラルは次のように設定されている。
 *
 * 04140000 -- UART0
 * 041C0000 -- UART4
 * 04310000 -- SD0
 * 04320000 -- SD1
 * 04400000 -- boot ROM
 * 05200000 -- SRAM
 * 70000000 -- PLIC
 * 74000000 -- CLINT
 * 80000000 -- DDR
 */
/*
 * カーネルメモリレイアウト:
 * ```
 *  MAXVA         -> ----------------------------
 *  (0x40_0000_0000)
 *                         トランポリン             R-X
 *  0x3F_FFFF_F000 -> ---------------------------
 *                         ガードページ             ---
 *                       カーネルスタック0          RW-
 *                         ガードページ             ---
 *                       カーネルスタック1          RW-
 *                              ....
 *  PHYSTOP       -> ----------------------------
 *  (0x8800_0000)            heap                   RW-
 *  PAGE_START    -> ----------------------------
 *  (0x8060_0000)     カーネル rodata, data, bss    RW-
 *                   ----------------------------
 *                         カーネル text            R-X
 *  KERNBASE      -> ----------------------------
 *  (0x8020_0000)
 *                   ----------------------------
 *                            CLINT                 RW-
 *   0x7400_0000  -> ----------------------------
 *                            PLIC                  RW-
 *   0x7000_0000  -> ----------------------------
 *                             RTC                  RW-
 *   0x0502_6000 -> ----------------------------
 *                             USB                  RW-
 *   0x0434_0000 -> ----------------------------
 *                             SD0                  RW-
 *   0x0431_0000  -> ----------------------------
 *                            UART0                 RW-
 *   0x0414_0000  -> ----------------------------
 *                            RESET                 RW-
 *   0x0300_3000  -> ----------------------------
 *                            CLKGEN                RW-
 *   0x0300_2000  -> ----------------------------
 *                            PINMUX                RW-
 *   0x0300_1000  -> ----------------------------
 *                            SYCTL                 RW-
 *   0x0300_0000  -> ----------------------------
 * ```
 */
/* Copyright (C) 2023 Jisheng Zhang <jszhang@kernel.org> */
/* config for CV1800b */
/* maximum number of CPUs */
/* core local interruptor (CLINT), which contains the timer. */
/* #define GPIO0 0x3020000 */
/* #define GPIO1 0x3021000 */
/* #define GPIO3 0x3023000 */
/* #define PWM0 0x3060000 */
/* #define PWM2 0x3062000 */
/* #define PWM3 0x3063000 */
/* core local interruptor (CLINT), which contains the timer. */
// #define CLINT 0x2000000L
/* qemu puts platform-level interrupt controller (PLIC) here. */
/* PHYSTOP: 0x83e0_0000 */
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
