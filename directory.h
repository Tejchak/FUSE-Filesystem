#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "blocks.h"
#include "inode.h"
#include "slist.h"

#define DIR_NAME_LENGTH 48

typedef struct dirent {
  char name[DIR_NAME_LENGTH];
  int inum;
  int used;
  char _reserved[8];
} dirent_t;

int directory_lookup(inode_t *dd, const char *name);
int directory_put(inode_t *dd, const char *name, int inum);
int directory_delete(inode_t *dd, const char *name);
slist_t *directory_list(inode_t *dd);
void print_directory(inode_t *dd);

#endif