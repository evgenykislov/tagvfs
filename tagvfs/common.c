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


struct qstr alloc_qstr_from_2str(const char* str1, size_t len1, const char* str2,
    size_t len2) {
  struct qstr r;
  void* m = NULL;

  if (!len1 || !str1) {
    return alloc_qstr_from_str(str2, len2);
  }

  if (!len2 || !str2) {
    return alloc_qstr_from_str(str1, len1);
  }

  r.name = NULL;
  r.len = len1 + len2;

  m = kmalloc(r.len, GFP_KERNEL);
  if (!m) {
    return r;
  }
  memcpy(m, str1, len1);
  memcpy(m + len1, str2 , len2);
  r.name = m;
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


struct qstr get_null_qstr() {
  struct qstr res;
  res.name = NULL;
  res.len = 0;
  return res;
}


struct qstr qstr_trim_header_if_exist(const struct qstr source, const struct qstr header) {
  size_t newlen;

  if (header.len > source.len || header.len == 0) { return get_null_qstr(); }
  if (memcmp(source.name, header.name, header.len) != 0) { return get_null_qstr(); }

  newlen = source.len - header.len;
  if (newlen == 0) {
    return get_null_qstr();
  }

  return alloc_qstr_from_str(source.name + header.len, newlen);
}


struct qstr qstr_add_header(const struct qstr source, const struct qstr header) {
  return alloc_qstr_from_2str(header.name, header.len, source.name, source.len);
}
