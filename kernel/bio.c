// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//有一个疑惑：为什么不能像前面哪个一样，每个cpu分配一些buf块
//链表设计：空闲buf是放在head后面
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13 //桶的数目
struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf hashbucket[NBUCKETS];
} bcache;

//

int
hash(int blockno){
  return blockno % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;
  for(int i=0; i<NBUCKETS; i++){
    initlock(&bcache.lock[i], "bcache_bucket");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  //和之前的初始化差不多，但是这里相当于hashbucket[i]都对应一个head
  // Create linked list of buffers
  //开始的时候，buffer没有blockno，buffer没有和磁盘块对应，放到第一个哈希桶里面去
    for(b = bcache.buf; b < bcache.buf+NBUF; b++){
      b->next = bcache.hashbucket[0].next;
      b->prev = &bcache.hashbucket[0];
      initsleeplock(&b->lock, "buffer_");
      bcache.hashbucket[0].next->prev = b;
      bcache.hashbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
//函数作用是，首先遍历cache buf 判断是否磁盘块被存入buffer中
//然后是得到bcache的spinlock（这个是为了防止其它进程返回相同的buf）
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int h = hash(blockno); //求对应的哈希桶
  acquire(&bcache.lock[h]); 

  // Is the block already cached?
  //在blockno对应的bucket里面找
  for(b = bcache.hashbucket[h].prev; b != &bcache.hashbucket[h]; b = b->prev){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++; //引用计数+1
      release(&bcache.lock[h]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //重新遍历一遍找引用次数为0的buf
  for(b = bcache.hashbucket[h].prev; b != &bcache.hashbucket[h]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[h]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  //如果是用hashbucket实现的话，如果没有找到这个对应的bucket
  //这个时候需要找一个引用次数为0的buf；
  //比如说一开始所有buf全在hashbucket[0]的情况，给一个blockno=2如果没有找到需要
  //分配的话，要从hashbucket[0]中找到空闲buf，然后放到hashbucket[2]中
  //这样需要的操作有，hashbucket[0]中要删掉这个buf，[2]中要添加这个buf
  for(int i= 0; i < NBUCKETS ; i++ ){ //从所有的hashbucket里面去找
  //问题在于，这样子操作，如果i = h，会导致两次acquire同一个锁出现，即出现死锁的情况，为了避免，需要避开h
      if(i == h)
        continue;
      acquire(&bcache.lock[i]);
      for(b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev)//
      {
        if(b->refcnt == 0){ //如果找到了
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          //更改属性

          b->next->prev=b->prev;
          b->prev->next=b->next; //断开链接
          release(&bcache.lock[i]);//释放的是待裁剪的哈希桶的锁
          //头节点后插入的方法
          //插入到第h个哈希桶
          b->next=bcache.hashbucket[h].next; //让b的下一个节点成为原来头节点后的哪个节点
          b->prev=&bcache.hashbucket[h]; //b的前一个节点成为头节点
          bcache.hashbucket[h].next->prev=b;//原来的头节点后的下一个节点(第二个) 成为第三个，相当于b被插入到第二个
          bcache.hashbucket[h].next=b;
          release(&bcache.lock[h]); //释放的是待插入的哈希桶的锁
          acquiresleep(&b->lock);
          return b;
        }

      }
   //没有找到也需要释放掉待裁剪的哈希桶的锁
   release(&bcache.lock[i]);
  }
  // release(&bcache.lock[h]);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;
//加，释放锁的过程在bget函数里面
  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int h = hash(b->blockno);//待释放的buf里面已经填充好了信息
  acquire(&bcache.lock[h]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next; //取下b

    b->next = bcache.hashbucket[h].next;
    b->prev = &bcache.hashbucket[h];
    bcache.hashbucket[h].next->prev = b; 
    bcache.hashbucket[h].next = b; //放到头节点后面
  }
  
  release(&bcache.lock[h]);
}

void
bpin(struct buf *b) {
  int h = hash(b->blockno);
  acquire(&bcache.lock[h]);
  b->refcnt++;
  release(&bcache.lock[h]);
}

void
bunpin(struct buf *b) {
  int h = hash(b->blockno);
  acquire(&bcache.lock[h]);
  b->refcnt--;
  release(&bcache.lock[h]);
}


