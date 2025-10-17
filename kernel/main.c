#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "emmc.h"
#include "include/vfs.h"
#include "include/vfsmount.h"
#include "include/list.h"
#include "spinlock.h"
#include "include/time.h"

volatile static int started = 0;
volatile static unsigned long main_hartid = ~0UL;

extern volatile unsigned long uart_base;
extern char _bss_start[], _bss_end[];

static void
printdate()
{
  static struct timeval tv;
  static struct tm tm;
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tm);
  printf("%d/%d/%d %d:%d:%d\n",
    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}


static void initfss(void);

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(main_hartid == ~0UL){
    memset(_bss_start, 0, _bss_end - _bss_start);
    main_hartid = cpuid();
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("lambt9 porting xv6\n");
    printf("\n");
    sbiinit();
    printf("Done SBI init\n");
    kinit();         // physical page allocator
    printf("Done kinit\n");
    kvminit();       // create kernel page table
    printf("Done kvminit\n");
    kvminithart();   // turn on paging
    printf("Done kvminithart\n");
    uart_base = UART0;
    __sync_synchronize();
    procinit();      // process table
    printf("Done procinit\n");
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    //virtio_disk_init(); // emulated hard disk
    ramdiskinit();
    sd_init();
    /* New init for VFS implementation */
    initvfssw();     // vfs table init  
    initvfsmlist();  // vfs mount list init
    mountinit();     // mount table init
    printf("Done mountinit\n");
    initfss();       // file systems
    installrootfs();
    printf("Done initfss\n");

#ifdef GPIO_DRIVER
    gpioinit();
#endif
#ifdef PWM_DRIVER
    pwminit();
#endif
#ifdef ADC_DRIVER
    adcinit();
#endif
#ifdef I2C_DRIVER
    i2cinit();
#endif
#ifdef SPI_DRIVER
    spiinit();
#endif
    rtc_init();
    printf("Done rtc_init\n");
    printdate();
    phy_init();
    eth_init();
    printf("Do userinit\n");
    userinit();      // first user process
    printf("Done userinit\n");
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
    printf("hart %d started\n", cpuid());
  }

  scheduler();        
}


static void
initfss(void) {
  // Init the supported filesystems
  if (inits5fs() != 0) // init s5 fs
    panic("S5 not registered");
  if (initext2fs() != 0) // init ext2 fs
    panic("ext2 not registered");
}


// int initext2fs(void) {
//   printf("Registering ext2 filesystem...\n");
//   printf("Ext2 fs registered\n");
//   return 0;
// }
