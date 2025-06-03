// inode.c
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "inode.h"
#include "blocks.h"
#include "bitmap.h"
#include <errno.h> //include this import to return errors so fuse can recognise failures (would just read 1 as a number of bytes)

#define INODE_BLOCK 1
#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(inode_t))
#define MAX_INODES INODES_PER_BLOCK

// Get a pointer to the inode at index inum
inode_t* get_inode(int inum) {
  if (inum < 0 || inum >= MAX_INODES) {
    return NULL;
  }
  void* base = blocks_get_block(INODE_BLOCK);
  return ((inode_t*)base) + inum;
}

//allocates a freee inode
int alloc_inode() {
  void* bm = get_inode_bitmap();
  for (int i = 1; i < MAX_INODES; i++) {
      if (!bitmap_get(bm, i)) {
          bitmap_put(bm, i, 1);
          inode_t* node = get_inode(i);
          memset(node, 0, sizeof(inode_t));
          node->refs = 1;
          return i;
      }
  }
  return -ENOSPC;
}

// frees an inode and release all of its blocks.
void free_inode(int inum) {
  inode_t* node = get_inode(inum);
  if (!node || node->refs <= 0) {
    return;
  }

  // free direct blocks
  int nblocks = (node->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  for (int b = 0; b < nblocks && b < NDIRECT; b++) {
      if (node->direct[b] != 0) {
          free_block(node->direct[b]);
      }
  }
  // free indirect blocks
  if (nblocks > NDIRECT && node->indirect != 0) {
      int* iblock = (int*)blocks_get_block(node->indirect);
      int rem = nblocks - NDIRECT;
      for (int i = 0; i < rem && i < NINDIRECT; i++) {
          if (iblock[i] != 0) free_block(iblock[i]);
      }
      free_block(node->indirect);
  }
  memset(node, 0, sizeof(inode_t));
  bitmap_put(get_inode_bitmap(), inum, 0);
}

//Grow an inode to at least new_size bytes by allocating additional blocks
int grow_inode(inode_t* node, int new_size) {
  int old_blocks = (node->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  int new_blocks = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  for (int b = old_blocks; b < new_blocks; b++) {
      int bnum = alloc_block();
      if (bnum < 0) {
        return -ENOSPC;
      }
      if (b < NDIRECT) {
          node->direct[b] = bnum;
      } else {
          // first time indirect needed
          if (node->indirect == 0) {
              node->indirect = alloc_block();
              if (node->indirect < 0) {
                return -ENOSPC;
              }
              // zero out the indirect block
              memset(blocks_get_block(node->indirect), 0, BLOCK_SIZE);
          }
          int* iblock = (int*)blocks_get_block(node->indirect);
          iblock[b - NDIRECT] = bnum;
      }
  }
  node->size = new_size;
  return 0;
}

// Shrink an inode to new_size bytes by freeing blocks no longer needed
int shrink_inode(inode_t* node, int new_size) {
  int old_blocks = (node->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  int new_blocks = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  // free blocks above new_blocks
  for (int b = new_blocks; b < old_blocks; b++) {
      if (b < NDIRECT) {
          free_block(node->direct[b]);
          node->direct[b] = 0;
      } else if (node->indirect) {
          int* iblock = (int*)blocks_get_block(node->indirect);
          free_block(iblock[b - NDIRECT]);
          iblock[b - NDIRECT] = 0;
      }
  }
  // if we dropped back below direct threshold, free indirect block
  if (new_blocks <= NDIRECT && node->indirect) {
      free_block(node->indirect);
      node->indirect = 0;
  }
  node->size = new_size;
  return 0;
}

//get the bnum for a inode
int inode_get_bnum(inode_t* node, int file_block) {
  if (file_block < 0) {
    return -EINVAL;
  }
  if (file_block < NDIRECT) {
      return node->direct[file_block];
  }
  if (node->indirect == 0) {
    return -EFBIG;
  }
  int* iblock = (int*)blocks_get_block(node->indirect);
  int idx = file_block - NDIRECT;
  if (idx < 0 || idx >= NINDIRECT) {
    return -EFBIG;
  }
  return iblock[idx];
}

//Print the inode
void print_inode(inode_t* node) {
  printf("INODE {refs: %d, mode: %04o, size: %d, direct[0]=%d, indirect=%d}\n",
         node->refs, node->mode, node->size,
         node->direct[0], node->indirect);
}