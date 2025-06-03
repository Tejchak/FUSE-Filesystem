#include <string.h>
#include <stdio.h>
#include <errno.h> //include this import to return errors so fuse can recognise failures (would just read 1 as a number of bytes)
#include "directory.h"
#include "blocks.h"
#include "inode.h"  // for inode_get_bnum

// Look up a file name inside a given inode
int directory_lookup(inode_t *dd, const char *name) {
  if (!dd) {
    return -1;
  }
  int bnum = inode_get_bnum(dd, 0);
  if (bnum <= 0) {
    return -1;
  }
  dirent_t *entries = (dirent_t *) blocks_get_block(bnum);
  int n_entries = dd->size / sizeof(dirent_t);

  for (int i = 0; i < n_entries; i++) {
    if (entries[i].used && strcmp(entries[i].name, name) == 0) {
      return entries[i].inum;
    }
  }

  return -1;
}

// Add to the directory
int directory_put(inode_t *dd, const char *name, int inum) {
  if (!dd || strlen(name) >= DIR_NAME_LENGTH) {
    return -1;
  }
  if (directory_lookup(dd, name) >= 0) {
    return -EEXIST;
  }

  int bnum = inode_get_bnum(dd, 0);
  if (bnum <= 0) {
    bnum = alloc_block();
    if (bnum < 0) {
      return -ENOSPC;
    }
    // give to first direct slot
    dd->direct[0] = bnum;
  }

  dirent_t *entries = (dirent_t *) blocks_get_block(bnum);
  int n_entries = dd->size / sizeof(dirent_t);

  // Look for a free slot
  for (int i = 0; i < n_entries; i++) {
    if (!entries[i].used) {
      strncpy(entries[i].name, name, DIR_NAME_LENGTH);
      entries[i].name[DIR_NAME_LENGTH - 1] = '\0';
      entries[i].inum = inum;
      entries[i].used = 1;
      return 0;
    }
  }

  // Otherwise, append to end
  dirent_t *entry = &entries[n_entries];
  strncpy(entry->name, name, DIR_NAME_LENGTH);
  entry->name[DIR_NAME_LENGTH - 1] = '\0';
  entry->inum = inum;
  entry->used = 1;
  dd->size += sizeof(dirent_t);
  return 0;
}

// delete entry in directory
int directory_delete(inode_t *dd, const char *name) {
  if (!dd) {
    return -1;
  }
  int bnum = inode_get_bnum(dd, 0);
  if (bnum <= 0) {
    return -1;
  }

  dirent_t *entries = (dirent_t *) blocks_get_block(bnum);
  int n_entries = dd->size / sizeof(dirent_t);

  for (int i = 0; i < n_entries; i++) {
    if (entries[i].used && strcmp(entries[i].name, name) == 0) {
      entries[i].used = 0;
      return 0;
    }
  }

  return -1;
}

// get the list of file names in directory
slist_t *directory_list(inode_t *dd) {
  if (!dd) {
    return NULL;
  }
  int bnum = inode_get_bnum(dd, 0);
  if (bnum <= 0) {
    return NULL;
  }
  dirent_t *entries = (dirent_t *) blocks_get_block(bnum);
  int n_entries = dd->size / sizeof(dirent_t);
  slist_t *list = NULL;

  for (int i = 0; i < n_entries; i++) {
    if (entries[i].used) {
      list = s_cons(entries[i].name, list);
    }
  }

  return list;
}

//print out the directory
void print_directory(inode_t *dd) {
  slist_t *list = directory_list(dd);
  slist_t *cur = list;
  printf("Directory contents:\n");
  while (cur) {
    printf("  - %s\n", cur->data);
    cur = cur->next;
  }
  s_free(list);
}