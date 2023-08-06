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


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
// lab 8
#define NBUK 13
#define hash(dev,blockno)((dev*blockno)%NBUK) // 取模

struct bucket{
  struct spinlock lock;
  struct buf head;
};
//
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct bucket buckets[NBUK]; // 13 buckets
} bcache;

void
binit(void)
{
  struct buf *b;
  struct buf *prev_b;

  initlock(&bcache.lock, "bcache");
  
  for(int i = 0; i < NBUK; i++){
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    bcache.buckets[i].head.next = (void*)0; // 设0
    if (i == 0){
      prev_b = &bcache.buckets[i].head;
      for(b = bcache.buf; b < bcache.buf + NBUF; b++){
        if(b == bcache.buf + NBUF - 1) // buf[29]
          b->next = (void*)0; // 设0
        prev_b->next = b;
        b->timestamp = ticks; // 初始化时间戳
        initsleeplock(&b->lock, "buffer");
        prev_b = b; 
      }    
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = hash(dev, blockno); // 获取哈希值作为id

  // Is the block already cached?
  acquire(&bcache.buckets[id].lock);  
  b = bcache.buckets[id].head.next; 
  while(b){ // 遍历bucket[id]
    if(b->dev == dev && b->blockno == blockno){ // 找到
      b->refcnt++; 
      release(&bcache.buckets[id].lock);
      acquiresleep(&b->lock);
      return b;   
    }
    b = b->next;
  }
  release(&bcache.buckets[id].lock);
  
  // 未 cached.
  int max_timestamp = 0; 
  int lru_id = -1; //
  int is_better = 0; // 暂无更好地lru获取的id
  struct buf *lru_b = (void*)0;
  struct buf *pre_lru_b = (void*)0;
  
  struct buf *pre_b = (void*)0;
  for(int i = 0; i < NBUK; i++){
    pre_b = &bcache.buckets[i].head;
    acquire(&bcache.buckets[i].lock);
    while(pre_b->next){
      // we must use ">=" max_timestamp == 0 at first time in eviction
      if(pre_b->next->refcnt == 0 && pre_b->next->timestamp >= max_timestamp){ 
        max_timestamp = pre_b->next->timestamp;
        is_better = 1;
        pre_lru_b = pre_b; // get prev_lru_b
        // not use break, we must find the max_timestamp until we running in the tail of buckets[12]
      }
      pre_b = pre_b->next;
    }
    if(is_better){
      if(lru_id != -1)
        release(&bcache.buckets[lru_id].lock); // release old buckets[lru_buk_id]
      lru_id = i; // get new lru_buk_id and alway lock it 
    }
    else
      release(&bcache.buckets[i].lock); // not better lru_buk_id, so we release current bucket[i]
    is_better = 0; // reset is_better to go next bucket[i]
  }

  //获取 lru_b
  lru_b = pre_lru_b->next; 

  // steal lru_b from buckets[lru_buk_id] by prev_lru_b
  // and release buckets[lru_buk_id].lock
  if(lru_b){
    pre_lru_b->next = pre_lru_b->next->next;
    release(&bcache.buckets[lru_id].lock);
  }

  // cache lru_b to buckets[buk_id]
  // atomic: one bucket.lock only be acquired by one process
  acquire(&bcache.lock);
  acquire(&bcache.buckets[id].lock);
  if(lru_b){
    lru_b->next = bcache.buckets[id].head.next;
    bcache.buckets[id].head.next = lru_b;
  }

  // if two processes will use the same block(same blockno) in buckets[lru_buk_id] 
  // one process can check it that if already here we get it
  // otherwise, we will use same block in two processes and cache double
  // then "panic freeing free block"
  b = bcache.buckets[id].head.next; // the first buf in buckets[buk_id]
  while(b){ 
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[id].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;   
    }
    b = b->next;
  }

  // not find lru_b when travel each bucket
  if (lru_b == 0)
    panic("bget: no buffers");
  
  lru_b->dev = dev;
  lru_b->blockno = blockno;
  lru_b->valid = 0;
  lru_b->refcnt = 1;
  release(&bcache.buckets[id].lock);
  release(&bcache.lock);
  acquiresleep(&lru_b->lock);
  return lru_b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

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

  int id = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt--;
  // 更新时间戳
  if(b->refcnt == 0)
    b->timestamp = ticks; 
  release(&bcache.buckets[id].lock);
}

void
bpin(struct buf *b) {
  int id = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt++;
  release(&bcache.buckets[id].lock);
}

void
bunpin(struct buf *b) {
  int id = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt--;
  release(&bcache.buckets[id].lock);
}


