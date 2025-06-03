#include <string.h>
#include <stdio.h>
#include <errno.h> //include this import to return errors so fuse can recognise failures (would just read 1 as a number of bytes)
#include <sys/stat.h>
#include "storage.h"
#include "blocks.h"
#include "inode.h"
#include "directory.h"
#include <sys/stat.h>     
#include <stdlib.h>      
#include "slist.h"       


int path_lookup(const char *path);
int path_parent(const char *path, char **name_out);

//Initialize the block from the file at path
void storage_init(const char *path) {
  blocks_init(path);

  inode_t *root = get_inode(0);
  if (root->refs == 0) {
    root->refs = 1;
    root->mode = 040755;
    root->size = 0;
    root->direct[0] = alloc_block();
    printf("+ initialized root directory\n");
  }

  //if for some reason I can't find the root directory inode I seed 
  // a default hello.txt file in the root
  //basically I realized that once I have a file in the mnt folder I can do things like normal
  //so I do that here upon initialization, don't fully understand why it works but it does
  if (directory_lookup(root, "hello.txt") < 0) {
    // allocate inode
    int h_inum = alloc_inode();
    inode_t *h_node = get_inode(h_inum);

    h_node->refs  = 1;
    h_node->mode  = 0100644;     // regular file, rw-r--r--
    h_node->size  = 6;           // length of "hello\n"
    h_node->direct[0] = alloc_block();

    // write the data
    memcpy(blocks_get_block(h_node->direct[0]), "hello\n", 6);

    // link it into the root directory
    int rv = directory_put(root, "hello.txt", h_inum);
    printf("+ seeded hello.txt (inode %d) → dir put rv=%d\n", h_inum, rv);
  }  
}

/**
 * Look up the inode for path (which may be nested), and fill in the
 * stat struct st with its inode number, mode, size, and link count
 */
int storage_stat(const char *path, struct stat *st) {
  int inum = path_lookup(path);
  if (inum < 0) {
    return -ENOENT;
  }
  inode_t *node = get_inode(inum);
  st->st_ino   = inum;
  st->st_mode  = node->mode;
  st->st_size  = node->size;
  st->st_nlink = node->refs;
  return 0;
}

//Create a new filesystem object at path with the
// specified mode
int storage_mknod(const char *path, int mode) {
  char *name;
  int parent = path_parent(path, &name);
  if (parent < 0) {
    return parent;
  }
  inode_t *dir = get_inode(parent);
  if (directory_lookup(dir, name) >= 0) { 
    free(name); 
    return -EEXIST; 
  }

  int inum = alloc_inode();
  if (inum < 0) { 
    free(name); 
    return -ENOSPC; 
  }

  inode_t *node = get_inode(inum);
  node->refs = 1;
  node->mode = mode;
  node->size = 0;
  for(int i = 0; i < NDIRECT; i++) {
    node->direct[i] = 0;
  }
  node->indirect = 0;

  int rv = directory_put(dir, name, inum);
  free(name);
  return rv;
}

//Write size bytes from buf into the file at path starting at offset
int storage_write(const char *path, const char *buf, size_t size, off_t offset) {
  struct stat st;
  int rv = storage_stat(path, &st);
  if (rv < 0) {
    return rv;
  }
  inode_t *node = get_inode(st.st_ino);

  // first grow the file to cover [offset, offset+size)
  rv = grow_inode(node, (offset + size > node->size ? offset + size : node->size));
  if (rv < 0) {
    return rv;
  }
  size_t written = 0;
  while (written < size) {
    int file_blk = (offset + written) / BLOCK_SIZE;
    int blk_off = (offset + written) % BLOCK_SIZE;
    int chunk = BLOCK_SIZE - blk_off;
    if (chunk > size - written) {
      chunk = size - written;
    }
    int bnum = inode_get_bnum(node, file_blk);
    char *block = blocks_get_block(bnum);
    memcpy(block + blk_off, buf + written, chunk);

    written += chunk;
  }

  return written;
}

//Read up to size bytes from the file at path into buf starting at offset
int storage_read(const char *path, char *buf, size_t size, off_t offset) {
  struct stat st;
  int rv = storage_stat(path, &st);
  if (rv < 0) {
    return rv;
  }
  inode_t *node = get_inode(st.st_ino);

  if (offset >= node->size) {
    return 0;
  }
  size_t to_read = size;
  if (offset + to_read > node->size) {
    to_read = node->size - offset;
  }
  size_t done = 0;
  while (done < to_read) {
    int file_blk = (offset + done) / BLOCK_SIZE;
    int blk_off = (offset + done) % BLOCK_SIZE;
    int chunk = BLOCK_SIZE - blk_off;
    if (chunk > to_read - done) {
      chunk = to_read - done;
    }
    int bnum = inode_get_bnum(node, file_blk);
    char *block = blocks_get_block(bnum);
    memcpy(buf + done, block + blk_off, chunk);

    done += chunk;
  }

  return done;
}

// extend the file at path to exactly size bytes
int storage_truncate(const char *path, off_t size) {
  struct stat st;
  int rv = storage_stat(path, &st);
  if (rv < 0) {
    return rv;
  }

  inode_t *node = get_inode(st.st_ino);

  if (size < node->size) {
    return shrink_inode(node, size);
  } else if (size > node->size) {
    return grow_inode(node, size);
  }

  return 0;
}

//return a list of the names in the directory at path.
slist_t *storage_list(const char *path) {
  int inum = path_lookup(path);
  if (inum < 0) {
    return NULL;
  }
  return directory_list(get_inode(inum));
}


// unlink from directory and free its inode and block
int storage_unlink(const char *path) {
  char *name;  int parent = path_parent(path,&name);
  if (parent<0) {
    return parent;
  }
  inode_t *dir = get_inode(parent);

  int inum = directory_lookup(dir,name);
  if (inum<0) {
     free(name); 
     return -ENOENT; 
    }

  directory_delete(dir,name);
  free_inode(inum);
  free(name);
  return 0;
}

// Rename a file at the root directory
int storage_rename(const char *from, const char *to) {
  char *oldname, *newname;
  int p1 = path_parent(from, &oldname);
  int p2 = path_parent(to,   &newname);
  if (p1<0||p2<0) {
    return -EINVAL;
  }

  inode_t *d1 = get_inode(p1), *d2 = get_inode(p2);
  int inum = directory_lookup(d1, oldname);
  if (inum<0) { 
    free(oldname); free(newname); 
    return -ENOENT; 
  }
  if (directory_lookup(d2,newname)>=0) {
     free(oldname); free(newname); 
     return -EEXIST; 
    }

  directory_put(d2, newname, inum);
  directory_delete(d1, oldname);

  free(oldname); free(newname);
  return 0;
}

// walk through the filesystem tree for path and return its inode number
int path_lookup(const char *path) {
  if (strcmp(path, "/")==0) {
    return 0;
  }
  slist_t *parts = s_explode(path+1, '/');  // skip leading slash
  int inum = 0;                              // start at root
  for (slist_t *p = parts; p; p = p->next) {
    inode_t *dir = get_inode(inum);
    inum = directory_lookup(dir, p->data);
    if (inum < 0) { 
      s_free(parts); 
      return -ENOENT;
    }
  }
  s_free(parts);
  return inum;
}

//gets the parent of a path
// “/a/b/c” → returns parent inode (inum of /a/b) and malloc()’s *name
int path_parent(const char *path, char **name_out) {
  // explode components
  slist_t *parts = s_explode(path+1, '/');
  if (!parts) {
    return -EINVAL;
  }

  // find last
  slist_t *last = parts, *prev = NULL;
  while (last->next) { 
    prev = last; last=last->next; 
  }
  *name_out = strdup(last->data);

  // detach tail
  if (prev) {
    prev->next = NULL;
  }
  // rebuild parent path
  int parent_inum;
  if (!prev) {
    parent_inum = 0;
  } else {
    char buf[256] = "/";
    for (slist_t *p=parts; p; p=p->next) {
      strcat(buf, p->data);
      if (p->next) strcat(buf, "/");
    }
    parent_inum = path_lookup(buf);
  }
  s_free(parts);
  return parent_inum;
}



// Make a new directory at path
int storage_mkdir(const char *path, mode_t mode) {
  char *name;
  int parent = path_parent(path, &name);
  if (parent < 0) {
    return parent;
  }

  inode_t *dir = get_inode(parent);
  if (directory_lookup(dir, name) >= 0) {
     free(name); 
     return -EEXIST; 
    }

  int inum = alloc_inode();
  if (inum < 0) {
     free(name); 
     return -ENOSPC; 
    }

  inode_t *node = get_inode(inum);
  node->refs  = 1;
  node->mode  = mode | S_IFDIR;   // mark as directory
  node->size  = 0;
  node->direct[0] = alloc_block();    // allocate a block for its entries

  int rv = directory_put(dir, name, inum);
  free(name);
  return rv;
}

// delete a directory
int storage_rmdir(const char *path) {
  char *name;
  int parent = path_parent(path, &name);
  if (parent < 0) {
    return parent;
  }

  inode_t *dir = get_inode(parent);
  int inum = directory_lookup(dir, name);
  if (inum < 0) { 
    free(name); 
    return -ENOENT; 
  }

  inode_t *node = get_inode(inum);
  if (!S_ISDIR(node->mode))  {
     free(name); 
     return -ENOTDIR; 
    }
  directory_delete(dir, name);
  free_inode(inum);
  free(name);
  return 0;
}