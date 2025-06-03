// Inode manipulation routines.
//
#ifndef INODE_H
#define INODE_H
// Number of direct block pointers
#define NDIRECT 12
// Number of pointers stored in an indirect block
#define NINDIRECT (BLOCK_SIZE / sizeof(int))
#include "blocks.h"



typedef struct inode {
  int refs;  // reference count
  int mode;  // permission & type
  int size;  // bytes
  //changed this to handle larger files
  int direct[NDIRECT];     // direct block numbers (0 if unused)
  int indirect;            // block number of indirect block (0 if none)
} inode_t;

void print_inode(inode_t *node);
inode_t *get_inode(int inum);
int alloc_inode();
void free_inode();
int grow_inode(inode_t *node, int size);
int shrink_inode(inode_t *node, int size);
int inode_get_bnum(inode_t *node, int file_bnum);

#endif