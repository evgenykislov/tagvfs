#include "common.h"

#include <linux/fs.h>
#include <linux/slab.h>


void* super_block_storage(const struct super_block* sb) {
  return sb->s_fs_info;
}

void* inode_storage(const struct inode* nod) {
  return nod->i_sb->s_fs_info;
}

struct qstr alloc_qstr_from_str(const char* str, size_t len) {
  struct qstr r;
  void* m = NULL;

  r.name = NULL;
  r.len = 0;
  if (!len || !str) {
    return r;
  }

  m = kmalloc(len, GFP_KERNEL);
  if (!m) {
    return r;
  }
  memcpy(m, str, len);
  r.name = m;
  r.len = len;
  return r;
}

struct qstr alloc_qstr_from_qstr(const struct qstr s) {
  return alloc_qstr_from_str(s.name, s.len);
}


void free_qstr(struct qstr* str) {
  kfree(str->name);
  str->name = NULL;
  str->len = 0;
}

int compare_qstr(const struct qstr n1, const struct qstr n2) {
  if (n1.len != n2.len) {
    return 1;
  }
  return !!memcmp(n1.name, n2.name, n1.len);
}

