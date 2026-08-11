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

#define NBUCKETS 13

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock evict_lock;
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKETS];
} bcache;

static int
hash(uint dev, uint blockno)
{
  return (dev + blockno) % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.evict_lock, "bcache_evict");

  for(int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.buckets[i].lock, "bcache_bucket");
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
  }

for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    b->dev = 0;
    b->blockno = 0;
    b->refcnt = 0;

    struct buf *head = &bcache.buckets[0].head;
    b->next = head->next;
    b->prev = head;
    head->next->prev = b;
    head->next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int b_idx = hash(dev, blockno);
  struct bucket *target_bkt = &bcache.buckets[b_idx];

  // Is the block already cached?
  acquire(&target_bkt->lock);
  for(b = target_bkt->head.next; b != &target_bkt->head; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&target_bkt->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&target_bkt->lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.evict_lock);

  for(int i = 0; i < NBUCKETS; i++) {
    int idx = (b_idx + i) % NBUCKETS;
    struct bucket *bkt = &bcache.buckets[idx];
    acquire(&bkt->lock);
    for(b = bkt->head.next; b != &bkt->head; b = b->next) {
      if(b->refcnt == 0) {
        if(idx != b_idx) {
          b->next->prev = b->prev;
          b->prev->next = b->next;
          release(&bkt->lock);
          acquire(&target_bkt->lock);
          b->next = target_bkt->head.next;
          b->prev = &target_bkt->head;
          target_bkt->head.next->prev = b;
          target_bkt->head.next = b;
        }
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&target_bkt->lock);
        release(&bcache.evict_lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bkt->lock);
  }
  panic("bget: no buffers");
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

  int b_idx = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[b_idx].lock);
  b->refcnt--;
  release(&bcache.buckets[b_idx].lock);
}

void
bpin(struct buf *b) {
  int b_idx = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[b_idx].lock);
  b->refcnt++;
  release(&bcache.buckets[b_idx].lock);
}

void
bunpin(struct buf *b) {
  int b_idx = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[b_idx].lock);
  b->refcnt--;
  release(&bcache.buckets[b_idx].lock);
}


