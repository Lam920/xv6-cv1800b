#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "sd.h"
#include "config.h"

#include "emmc.h"
#include "include/designware.h"

#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fcntl.h"
#include "file.h"
#include "include/cache.h"  

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0_PHY, PGSIZE, PTE_DEVICE);

  // virtio mmio disk interface
#ifdef VIRTIO0
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_DEVICE);
#endif


#ifdef CV180X
  printf("Do kvm map for CV1800B\n");
  kvmmap(kpgtbl, MMIO_BASE, MMIO_BASE, PGSIZE, PTE_DEVICE);
  printf("Do kvm map for CLKGEN\n");
  kvmmap(kpgtbl, CLKGEN_BASE, CLKGEN_BASE, PGSIZE, PTE_DEVICE);
  printf("Do kvm map for PINMUX_BASE\n");
  kvmmap(kpgtbl, PINMUX_BASE, PINMUX_BASE, PGSIZE, PTE_DEVICE);
  printf("Do kvm map for RESET_BASE\n");
  kvmmap(kpgtbl, RESET_BASE, RESET_BASE, PGSIZE, PTE_DEVICE);
  // kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_DEVICE);
  printf("Do kvm map for SD0_BASE\n");
  kvmmap(kpgtbl, SD0_BASE, SD0_BASE, 16*PGSIZE, PTE_DEVICE);
  printf("Do kvm map for GPIO\n");
#endif

#ifdef GPIO0
  kvmmap(kpgtbl, GPIO0, GPIO0, PGSIZE, PTE_DEVICE);
#endif
#ifdef GPIO1
  kvmmap(kpgtbl, GPIO1, GPIO1, PGSIZE, PTE_DEVICE);
#endif
#ifdef GPIO2
  kvmmap(kpgtbl, GPIO2, GPIO2, PGSIZE, PTE_DEVICE);
#endif
#ifdef GPIO3
  kvmmap(kpgtbl, GPIO3, GPIO3, PGSIZE, PTE_DEVICE);
#endif

#ifdef PWM0
  kvmmap(kpgtbl, PWM0, PWM0, PGSIZE, PTE_DEVICE);
#endif
#ifdef PWM1
  kvmmap(kpgtbl, PWM1, PWM1, PGSIZE, PTE_DEVICE);
#endif
#ifdef PWM2
  kvmmap(kpgtbl, PWM2, PWM2, PGSIZE, PTE_DEVICE);
#endif
#ifdef PWM3
  kvmmap(kpgtbl, PWM3, PWM3, PGSIZE, PTE_DEVICE);
#endif

#ifdef ADC0
  kvmmap(kpgtbl, ADC0, ADC0, PGSIZE, PTE_DEVICE);
#endif

#ifdef I2C0
  kvmmap(kpgtbl, I2C0, I2C0, PGSIZE, PTE_DEVICE);
#endif

#ifdef SPI0
  kvmmap(kpgtbl, SPI0, SPI0, PGSIZE, PTE_DEVICE);
#endif

  kvmmap(kpgtbl, RTC_CTRL_BASE, RTC_CTRL_BASE, PGSIZE, PTE_DEVICE);
  kvmmap(kpgtbl, RTC_CORE_BASE, RTC_CORE_BASE, PGSIZE, PTE_DEVICE);

  printf("Map for ETH0_BASE\n");
  kvmmap(kpgtbl, ETH0_BASE, ETH0_BASE, PGSIZE, PTE_DEVICE);
  printf("Map for ETH0_DMA\n");
  kvmmap(kpgtbl, ETH0_BASE + DW_DMA_BASE_OFFSET, ETH0_BASE + DW_DMA_BASE_OFFSET, PGSIZE, PTE_DEVICE);

  // PLIC
  printf("Do kvm map for PLIC\n");
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_DEVICE);
  printf("Done kvm map for PLIC\n");

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_EXEC);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_NORMAL);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_EXEC);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  asm volatile("fence rw, rw");
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");
  
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V | PTE_A | PTE_D;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  sfence_vma();
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if((PTE_FLAGS(*pte) & 0x3ff) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
  sfence_vma();
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;
  printf("Do uvm first\n");
  if(sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_NORMAL|PTE_X|PTE_U);
  memmove(mem, src, sz);
  asm volatile("fence.i"); 
  printf("Done uvm first\n");
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_RO|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
#ifndef LAB_MMAP
      panic("freewalk: leaf");
#endif
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint64 flags;
#ifndef LAB_COW
  char *mem;
#endif

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
#ifdef LAB_COW
    /* [cow] also mark parent PTE as readonly*/
    // if (1) { cause error ??????? illegal instruction
    if (*pte & PTE_W) {
      *pte = (*pte) & (~PTE_W);
      *pte = (*pte) | PTE_COW;
    }
    pa = PTE2PA(*pte);
    /* [cow] copy all pte flags from parent to child */
    flags = PTE_FLAGS(*pte);
    /* [cow] map new child process pagetable pa memory same as parent */
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      goto err;
    }
    acquire(&cowlock);
    pgcount_arr[PAGECOUNT_IDX((uint64)pa)] += 1;
    release(&cowlock);
#endif

#ifndef LAB_COW
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
#endif
  }
  // Synchronize the instruction and data streams,
  // since we may copy pages with instructions.
  asm volatile("fence.i");
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;
  struct proc *p = myproc();

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA) {
      return -1;
    }
    pte = walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
      return -1;
    }
    // Handle COW pages
    if (*pte & PTE_COW) {
      char *mem;
      if((mem = kalloc()) == 0) {
        panic("Failed to allocate physical page for COW\n");
      }
      pa0 = PTE2PA(*pte);
      /* First copy original mapping page to newly allocated page */
      memmove(mem, (char*)pa0, PGSIZE);
      /* Update PTE: set to new physical address, make writable, clear COW flag */
      *pte = PA2PTE(mem) | PTE_FLAGS(*pte) | PTE_W;
      *pte = *pte & (~PTE_COW);
      /* Free COW mapping page (decrease reference) */
      kfree((void *)pa0);
      pa0 = (uint64)mem;
    } else {
      // Regular page - must be writable
      if((*pte & PTE_W) == 0) {
        printf("copyout: FAIL - not writable, pid=%d, dstva=%p, PTE=%p (COW=%d, W=%d)\n",
               p->pid, dstva, *pte, (*pte & PTE_COW) != 0, (*pte & PTE_W) != 0);
        return -1;
      }
      pa0 = PTE2PA(*pte);
    }
    
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}


uint64 sys_mmap(void) {
  /*
find an unused region in the process's address space 
in which to map the file, and add a VMA to the process's table
of mapped regions. 
*/
// rounddown addr
// find unused region
// the dump solution is loop each of them find the invalid one
// set the content
// In page fault, manually check memory region for each.

  uint64 addr;
  int size, prot, flags, fd, offset;

  argaddr(0, &addr);
  argint(1, &size);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, &offset);
  
  struct proc *p = myproc();

  printf("process pagetable: %p\n", p->pagetable);
  struct file *f = p->ofile[fd];
  // check that mmap doesn't allow read/write mapping of a
  // file opened read-only.
  if (flags & MAP_SHARED) {
    if (!(f->writable) && (prot & PROT_WRITE)) {
      printf("File is read-only, but we mmap with write permission and flag. %d vs %d\n",
            f->writable, prot);
      return 0xffffffffffffffff;
    }
  }

  uint64 cur_max = p->cur_max;
#ifdef DEBUG_MMAP
  printf("addr(%p), size(%d), prot(%d), flags(%d), fd(%d), offset(%d). Current Max(%p). MAXVA(%p)\n",
         (uint64 *)addr, size, prot, flags, fd, offset, (uint64 *)cur_max, (uint64 *)MAXVA);
#endif
  /* mmap: Start address for kernel to found VM to allocate for mmap */       
  uint64 start_addr = PGROUNDDOWN(cur_max - size);

  // find an unused vma
  struct vm_area_struct *vm = 0;
  for (int i=0; i<100; i++) {
    if (p->vma[i].valid == 0) {
      vm = &p->vma[i];
      break;
    }
  }
  if (vm) {
    vm->valid = 1;
    vm->start_ad = start_addr;
    vm->orig_start_ad = start_addr;  // Save original start for file offset calc
    vm->end_ad = cur_max;
    vm->len = size;
    vm->prot = prot;
    vm->flags = flags;
    vm->forked = 0;  // This is an original mmap, not inherited from fork
    vm->fd = fd;
    vm->file = p->ofile[fd];
    vm->file->ref++;

    // update cur_max: reset process current max available
    p->cur_max = start_addr;

#ifdef DEBUG_MMAP
    printf("mmap is set. max va(%p). VMA start(%p), end(%p). Inode(%d)\n", 
            (uint64 *)MAXVA, (uint64 *)vm->start_ad, (uint64 *)vm->end_ad, vm->file->ip->inum);
#endif
  } else {
    return 0xffffffffffffffff;
  }

  return start_addr;
}
uint64 sys_munmap(void) {
  uint64 addr;
  int size;

  argaddr(0, &addr);
  argint(1, &size);
  
  uint64 start_base = PGROUNDDOWN(addr);
  uint64 end_base = start_base + size;  // Fixed: should be start + size, not PGROUNDDOWN
  
  struct proc *p = myproc();
  struct vm_area_struct *vm = 0;
  for (int i=0; i<MMAP_PAGES; i++) {
    if (p->vma[i].valid == 1 && 
        p->vma[i].start_ad <=  start_base &&
        end_base <= p->vma[i].end_ad) {
      vm = &p->vma[i];
      break;
    }
  }
  if (!vm) {
    printf("Cannot found VMA. start base(%p), end base(%p)\n",
          (uint64 *)start_base, (uint64 *)end_base);
    return -1;
  }
  
  printf("munmap: addr=%p size=%d -> start_base=%p end_base=%p. VMA: start=%p end=%p\n",
         (uint64 *)addr, size, (uint64 *)start_base, (uint64 *)end_base,
         (uint64 *)vm->start_ad, (uint64 *)vm->end_ad);

  if (vm->flags & MAP_SHARED) {
    printf("....We need to write back file\n");
    
    // Skip writeback for forked VMAs - they shouldn't modify the shared file
    // because their pages are either COW copies or newly allocated after fork
    if (vm->forked) {
#ifdef DEBUG_MMAP
      printf("....VMA was forked, skipping writeback to preserve file integrity\n");
#endif
    } else {
      struct file *f = vm->file;
      
      // Check if any pages are COW - if so, skip writeback for those pages
      // because COW pages are private copies that shouldn't affect the shared file
      pte_t *pte;
      int has_non_cow_pages = 0;
      
      for(uint64 i = start_base; i < end_base; i += PGSIZE) {
        if((pte = walk(p->pagetable, i, 0)) != 0 && (*pte & PTE_V)) {
          if (!(*pte & PTE_COW)) {
            has_non_cow_pages = 1;
            // Flush cache for non-COW pages that will be written back
            uint64 pa = PTE2PA(*pte);
            flush_dcache_range(pa, pa + PGSIZE);
          }
        }
      }
      
      // Only write back if we have non-COW pages
      if (has_non_cow_pages) {
        begin_op();
        f->ip->iops->ilock(f->ip);
#ifdef DEBUG_MMAP   
        printf("Writeback: inode=%d, current size=%d\n", f->ip->inum, f->ip->size);
        // Debug: check what's actually in the pages before writeback
        for(uint64 i = start_base; i < end_base && i < start_base + PGSIZE; i += PGSIZE) {
          pte_t *check_pte = walk(p->pagetable, i, 0);
          if (check_pte && (*check_pte & PTE_V) && !(*check_pte & PTE_COW)) {
            uint64 check_pa = PTE2PA(*check_pte);
            char *check_ptr = (char *)check_pa;
            printf("Page at va=%p has pa=%p, first 8 bytes: %x %x %x %x %x %x %x %x\n",
                   (void*)i, (void*)check_pa,
                   check_ptr[0], check_ptr[1], check_ptr[2], check_ptr[3],
                   check_ptr[4], check_ptr[5], check_ptr[6], check_ptr[7]);
          }
        }
#endif
        
        // Write back only the unmapped region, respecting file size
        // Calculate the offset in the file for this unmapped region
        // Use orig_start_ad (original start) not current start_ad (which may have been modified)
        uint64 file_offset = start_base - vm->orig_start_ad;
        uint64 write_size = end_base - start_base;
        
        // Don't write beyond the original file size
        if (file_offset < f->ip->size) {
          if (file_offset + write_size > f->ip->size) {
            write_size = f->ip->size - file_offset;
          }
          printf("Writing back: offset=%ld, size=%ld, file_size=%d\n", 
                 file_offset, write_size, f->ip->size);
          f->ip->iops->writei(f->ip, 1, start_base, file_offset, write_size);
        }
        
        f->ip->iops->iunlock(f->ip);
        end_op();
      }
    }
  }

  // Unmap the pages from the page table
  pte_t *pte;
  for(uint64 i = start_base; i < end_base; i += PGSIZE){
    printf("munmap: unmap va %p\n", (uint64 *)i);
    if((pte = walk(p->pagetable, i, 0)) != 0) {  // Check if PTE exists
      if(*pte & PTE_V) {                          // Check if it's valid
        uvmunmap(p->pagetable, i, 1, 1);          // Unmap and free the page
      }
    }
  }

  // 4 cases
  // first part is un-map
  if (vm->start_ad == start_base && end_base < vm->end_ad) {
    vm->start_ad = end_base;
    vm->len -= size;
  } else if (vm->start_ad < start_base && end_base == vm->end_ad){
    // last part is un-map
    vm->end_ad = start_base;  // Fixed: should be start_base, not start_base - 1
    vm->len -= size;           // Fixed: should be -= not =
  } else if (vm->start_ad == start_base && vm->end_ad == end_base) {
    // exact size
    vm->file->ref--;
    //vm->file->off = 0;
    vm->valid = 0;
    vm->len = 0;
  } else if (vm->start_ad < start_base && end_base < vm->end_ad) {
    printf("this is very tricky...need to fix offset\n");
  } else {
    printf("Err. start base(%p), end base(%p). vm start(%p), vm end(%p)\n",
         (uint64 *)start_base, (uint64 *)end_base, (uint64 *)vm->start_ad, (uint64 *)vm->end_ad);
  }
  return size;
}

int mmap_read(struct file *f, char *pa, int off, int size) {
  f->ip->iops->ilock(f->ip);
  // read to kernel/physical address directly
  printf("mmap_read: read file inum(%d) at off(%d) to pa(%p) with size(%d)\n",
         f->ip->inum, off, (uint64 *)pa, size);
  int n = f->ip->iops->readi(f->ip, 0, (uint64)pa, off, size);
  printf("mmap_read: successfully read %d bytes\n", n);
  f->ip->iops->iunlock(f->ip);
  return n;
} 

void free_all_vma(pagetable_t pagetable, uint64 start, uint64 end) {
  pte_t *pte;
  for(int i = start; i <= end; i+=PGSIZE) {
    if((pte = walk(pagetable, i, 0)) == 0) {
      if(*pte & PTE_V) {
        uvmunmap(pagetable, i, PGSIZE, 0);
      }
    }
  }
}

void copy_vma(struct vm_area_struct *dst, struct vm_area_struct *src) {
  dst->valid = 1;
  dst->start_ad = src->start_ad;
  dst->orig_start_ad = src->orig_start_ad;  // Copy original start address
  dst->end_ad = src->end_ad;
  dst->len = src->len;
  dst->prot = src->prot;
  dst->flags = src->flags;
  dst->forked = 1;  // Mark as inherited from fork
  dst->fd = src->fd;
  dst->file = src->file;
  // Note: Don't increment ref here - caller (fork) will set dst->file
  // to the filedup'd version from np->ofile[dst->fd]
}
