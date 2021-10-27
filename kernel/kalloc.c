// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct  kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];
//之前只有一个锁的设计，会导致在多线程情景下，多个线程申请内存的时候会出现锁竞争
//如果对于每一个cpu都设计相应的空闲链表和锁，当线程a在申请内存的时候，如果线程b也申请内存
//这个时候线程b要获得的就是它自己所在cpu的锁，并不会产生竞争；
//当然，如果线程b所在cpu的freelist空了，并且这个时候向线程a申请的时候线程a也在分配内存，这个时候依然会出现锁竞争
//总的来看，锁的意义是限制资源在多线程情景下被修改，约束的是资源，而对于不同线程有不同的cpu，
//所以用cpu_id标记哪个cpu(线程)在用
void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  // freerange(end, (void*)PHYSTOP);
  for(int i = 0; i < NCPU; i++){
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

//初始化的时候使用，来把空闲内存页加到链表里
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
    //每次+PGSIZE保证是对每一页进行操作
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
  uint64 cpu_id;
  push_off();
  cpu_id =cpuid();
  pop_off();

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;//把释放的内存重新加入当前cpu的空闲链表中
  kmems[cpu_id].freelist = r; //把当前内存块加入到freelist,形成freemem链表
  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  // struct run *r;

  // acquire(&kmem.lock);
  // r = kmem.freelist;
  // if(r)
  //   kmem.freelist = r->next; //移除freelist的第一个元素，并返回这个元素
  // release(&kmem.lock);

  // if(r)
  //   memset((char*)r, 5, PGSIZE); // fill with junk
  // return (void*)r;
  struct run *r;
  uint64 cur_cpu;
  push_off(); //关闭中断
  cur_cpu = cpuid();
  pop_off();
  int cnt = 0;
  uint64 cpu_id = cur_cpu;
 //分配当前cpu的freelist内存块
 //没有则窃取其它cpu的freelist
  while(cnt <= NCPU)
  {
      acquire(&kmems[cpu_id].lock);
      r = kmems[cpu_id].freelist;
      if(r)
        kmems[cpu_id].freelist = r -> next;
      release(&kmems[cpu_id].lock);//不管r是空还是什么（有没有申请到内存），这个时候都需要释放锁
      if(r)//r不为空，返回r
      {
        memset((char *)r, 5, PGSIZE);
        return (void *)r;
      }
      cnt++;
      cpu_id = (cpu_id + 1 ) % NCPU;
  }
  return (void *)r;
 //否则返回0
}
//物理内存是在多进程之间共享的，
//所以不管是分配还是释放页面，每次操作kmem.freelist时都需要先申请kmem.lock，此后再进行内存页面的操作
