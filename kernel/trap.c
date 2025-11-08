#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "softirq.h"
#include "fcntl.h"
#include "include/cache.h"

struct spinlock tickslock;
uint ticks;

struct spinlock pendinglock;
uint64 pending;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();


static const char *scause_desc(uint64 stval);

static int check_mmap_vma(uint64 va, struct vm_area_struct *vma);

void
trapinit(void)
{
  initlock(&tickslock, "time");
  initlock(&pendinglock, "softirq");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if(r_scause() == 13 || r_scause() == 15){
#ifdef DEBUG_MMAP
    printf("usertrap(): mmap page fault %lx (%s) pid=%d\n", r_scause(), scause_desc(r_scause()), p->pid);
#endif
    handle_pagefault(r_scause());
  }else if(r_scause() == 2){
    printf("illegal instruction at: %p of pid: %d with name: %s\n", (uint64 *)myproc()->trapframe->epc, myproc()->pid, myproc()->name);
    printf("usertrap(): unexpected scause 0x%lx (%s) pid=%d\n", r_scause(), scause_desc(r_scause()), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    panic("Handle illegal\n");
  }else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause 0x%lx (%s) pid=%d\n", r_scause(), scause_desc(r_scause()), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}




static int handle_mmap_pagefault(uint64 va, int vma_idx, struct vm_area_struct *vm, int s_cause);

/* Handling page fault exception*/
#ifdef LAB_COW
int handle_pagefault(int s_cause) 
{
  /* [cow] get virtual address that caused page fault when write */
  uint64 va = r_stval();
  uint64 pa;
  struct proc *p = myproc();
  pte_t *pte;
  char *mem;
  if (va >= MAXVA) 
  {
    setkilled(p);
    return -1;
  }

  struct vm_area_struct vm;
  int vma_idx = check_mmap_vma(va, &vm);

  if (vma_idx >= 0) {
    handle_mmap_pagefault(va, vma_idx, &vm, s_cause);
    return 0;
  }
  
  if (s_cause == 13) {
    printf("pagefault: not in mmap region\n");
    setkilled(p);
    return -1;
  }
  /* [cow] at first, va must be aligned to PAGETABLE for not panic */
  // printf("***Before va: %p of pid: %d with name: %s****\n", (uint64 *)va, p->pid, p->name);
  va = PGROUNDDOWN(va);
  if((pte = walk(p->pagetable, va, 0)) == 0)
  {
    printf("pagefault: pte should exist\n");
    setkilled(p);
    return -1;
  }
  if((*pte & PTE_V) == 0)
  {
    printf("pagefault: page not present\n");
    setkilled(p);
  }
  /* [cow] get old mapping memory between child and parent.
  Now child want to write to this memory, so we need to copy to child
  and add perm */
  pa = PTE2PA(*pte);
  // [cow] copy all pte flags from parent to child
  // printf("pte flags: %p\n", (uint64 *)PTE_FLAGS(*pte));
  if ((*pte & PTE_COW) == 0) {
    printf("Real pagefault\n");
    setkilled(p);
    return -1;
  }
  if((mem = kalloc()) == 0)
      goto err;
  memmove(mem, (char*)pa, PGSIZE);
  /* [cow] Update pte to newly allocated physical memory */
  if (PTE_FLAGS(*pte) & PTE_COW) {
    *pte = PA2PTE(mem) | PTE_FLAGS(*pte) | PTE_W;
    *pte = *pte & (~PTE_COW);
  }
  /* [cow] Decrease ref count to pagetable*/
  kfree((void *)pa);
  return 0;
err:
  uvmunmap(p->pagetable, 0, va / PGSIZE, 1);
  printf("Handle pagefault error\n");
  return -1;
}
#endif


//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
  net_timer_handler();
}

void
softintr()
{
  acquire(&pendinglock);
  uint64 irqs = pending;
  pending = 0;
  release(&pendinglock);
  printf("softintr is fired\n");

  if(irqs & SOFT_IRQ_NET_RX) {
    net_softirq_handler();
  }
  if(irqs & SOFT_IRQ_NET_EVENT) {
    net_event_handler();
  }

  w_sip(r_sip() & ~SIP_SSIP);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    //} else if(irq == VIRTIO0_IRQ){
    //  virtio_disk_intr();
    } else if (irq == SD0_IRQ) {
      //sd_intr();
    } 
    else if (irq == ETH0_IRQ) {
      // dw ethernet interrupt
      printf("[plic] got eth irq=%d\n", irq);
      eth_intr();
    }
    else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);
    return 1;
  } 
  else if(scause == 0x8000000000000001L){
    // software interrupt.
    softintr();
    return 1;
#ifdef CONFIG_RISCV_M_MODE
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by OpenSBI or timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);
    return 2;
#else
  } else if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 5){
    // lambt9: Supervisor timer interrupt
    // S-mode timer interrupt,
    // OpenSBI will STORE PLIC by itself
    unsigned long next;

    csr_clear(CSR_IE, 1 << 5);
    clockintr();
    next = csr_read(CSR_TIME) + INTERVAL;
    sbi_set_timer(next);
    csr_set(CSR_IE, 1 << 5);

    return 2;
#endif
  } else {
    return 0;
  }
}


static const char *
scause_desc(uint64 stval)
{
  static const char *intr_desc[16] = {
    [0] "user software interrupt",
    [1] "supervisor software interrupt",
    [2] "<reserved for future standard use>",
    [3] "<reserved for future standard use>",
    [4] "user timer interrupt",
    [5] "supervisor timer interrupt",
    [6] "<reserved for future standard use>",
    [7] "<reserved for future standard use>",
    [8] "user external interrupt",
    [9] "supervisor external interrupt",
    [10] "<reserved for future standard use>",
    [11] "<reserved for future standard use>",
    [12] "<reserved for future standard use>",
    [13] "<reserved for future standard use>",
    [14] "<reserved for future standard use>",
    [15] "<reserved for future standard use>",
  };
  static const char *nointr_desc[16] = {
    [0] "instruction address misaligned",
    [1] "instruction access fault",
    [2] "illegal instruction",
    [3] "breakpoint",
    [4] "load address misaligned",
    [5] "load access fault",
    [6] "store/AMO address misaligned",
    [7] "store/AMO access fault",
    [8] "environment call from U-mode",
    [9] "environment call from S-mode",
    [10] "<reserved for future standard use>",
    [11] "<reserved for future standard use>",
    [12] "instruction page fault",
    [13] "load page fault",
    [14] "<reserved for future standard use>",
    [15] "store/AMO page fault",
  };
  uint64 interrupt = stval & 0x8000000000000000L;
  uint64 code = stval & ~0x8000000000000000L;
  if (interrupt) {
    if (code < NELEM(intr_desc)) {
      return intr_desc[code];
    } else {
      return "<reserved for platform use>";
    }
  } else {
    if (code < NELEM(nointr_desc)) {
      return nointr_desc[code];
    } else if (code <= 23) {
      return "<reserved for future standard use>";
    } else if (code <= 31) {
      return "<reserved for custom use>";
    } else if (code <= 47) {
      return "<reserved for future standard use>";
    } else if (code <= 63) {
      return "<reserved for custom use>";
    } else {
      return "<reserved for future standard use>";
    }
  }
}

static int check_mmap_vma(uint64 va, struct vm_area_struct *vm) {
  struct proc *p = myproc();
  for (int i=0; i<100; i++) {
    if (p->vma[i].valid == 1) {
      if (va >= p->vma[i].start_ad && va < p->vma[i].end_ad) {
        *vm = p->vma[i];
        return i;
      }
    }
  }
  return -1;
}

static int handle_mmap_pagefault(uint64 va, int vma_idx, struct vm_area_struct *vm, int s_cause) {
  struct proc *p = myproc();
  // allocate a page of physical memory.
  if (!vm) {
    printf("VM addr is not lived in VMA, error out.\n");
    p->killed = 1;
    return -1;
  }
  
  // Check for write to read-only mapping
  if (s_cause == 15) {  // Store/AMO page fault (write attempt)
    if ((vm->prot & PROT_WRITE) == 0) {  // But VMA is read-only
      printf("mmap: write to read-only mapping\n");
      p->killed = 1;
      return -1;
    }
  }
  
  uint64 fault_addr_head = PGROUNDDOWN(va);
  pte_t *pte;
  
  // Check if page is already mapped (might happen in fork scenarios)
  if((pte = walk(p->pagetable, fault_addr_head, 0)) != 0 && (*pte & PTE_V)) {
    printf("Mmap page already mapped at %lx\n", fault_addr_head);
    return 0;
  }
  
  char *pa = kalloc();
  if(pa == 0)
    panic("kalloc");
  memset(pa, 0, PGSIZE);

#ifdef DEBUG_MMAP
  printf("Mmap page fault at addr: %lx and addr_head: %lx, allocate start_ad: %lx\n", va, fault_addr_head, vm->start_ad);
#endif
  // read 4096 bytes of the relevant file into physical memory BEFORE mapping.
  // IMPORTANT: the read offset is the distance between page_fault_addr
  // and VMA->orig_start_ad (original start address, not current start_ad).
  int distance = fault_addr_head - vm->start_ad;
  mmap_read(vm->file, pa, distance, PGSIZE);
  
   
  // Flush and invalidate cache to ensure coherency between kernel write and user read
  flush_dcache_range((unsigned long)pa, (unsigned long)pa + PGSIZE);
  invalidate_dcache_range((unsigned long)pa, (unsigned long)pa + PGSIZE);
  
  // Ensure memory is synchronized before mapping to user space
  __sync_synchronize();

  // Now map the page with the file content into user space
  if (mappages(p->pagetable, fault_addr_head, PGSIZE, (uint64)pa, vm->prot | PTE_U) != 0) {
    kfree(pa);
    p->killed = 1;
    return -1;
  }

  // Check content using physical address (pa), not user virtual address
  char *t = (char *)pa;
  printf("DEBUG: After mmap_read, pa=%p, first bytes: %x %x %x %x\n", 
         pa, t[0], t[1], t[2], t[3]);
  if (t[0] != 'A') {
    printf("mismatch!!!! wanted 'A', got %x\n", t[0]);
  }
  
#ifdef DEBUG_MMAP
  printf("Trap addr base(%lx). VMA start(%lx), end(%lx) with distance: %x.\n", 
        fault_addr_head, vm->start_ad, vm->end_ad, distance);
#endif
  return 0;
}

