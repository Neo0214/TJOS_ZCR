// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};
// lab 8
struct kmem kmems[NCPU];
//
void
kinit()
{ // lab 8
  for (int i=0;i<NCPU;i++)
  {
    initlock(&kmems[i].lock, "kmem");
    if (i==0)
      freerange(end, (void*)PHYSTOP);
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  // lab 8
  push_off();
  int cpuId=cpuid();
  pop_off();
  // continue
  acquire(&kmems[cpuId].lock);
  r->next = kmems[cpuId].freelist;
  kmems[cpuId].freelist = r;
  release(&kmems[cpuId].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  // lab 8
  push_off();
  int cpuId=cpuid();
  pop_off();
  // continue
  acquire(&kmems[cpuId].lock);
  r = kmems[cpuId].freelist;
  // if freelist of current cpu_id has freepage we use it
  if(r){
    kmems[cpuId].freelist = r->next;
    release(&kmems[cpuId].lock);
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  }

  // else we steal freepage from other freelist of cpus
  else{
    release(&kmems[cpuId].lock);
    for(int i = 0; i < NCPU; i++){
      // avoid race condition(same cpu_id lock)
      if (i != cpuId){ 
        acquire(&kmems[i].lock);

        // the last_list also not memory so ret 0 
        if(i == NCPU - 1 && kmems[i].freelist == 0){  
          release(&kmems[i].lock);
          return (void*)0;
        }

        // It is OK to allocte a freepage by stealing from other cpus
        if(kmems[i].freelist){
          struct run *to_alloc = kmems[i].freelist; 
          kmems[i].freelist = to_alloc->next;
          release(&kmems[i].lock);
          memset((char*)to_alloc, 5, PGSIZE); // fill with junk
          return (void*)to_alloc;
        }

        // if no matching condition, we must release current lock
        release(&kmems[i].lock); 
      }
    }
  }

  // Returns 0 if the memory cannot be allocated. 
  return (void*)0;
}
