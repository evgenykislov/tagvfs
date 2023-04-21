#include "tag_storage.h"

struct qstr tag1 = QSTR_INIT("Классика", 8);
struct qstr tag2 = QSTR_INIT("Винтаж", 6);
struct qstr tag3 = QSTR_INIT("Модерн", 6);
struct qstr fname1 = QSTR_INIT("m1.mp4", 6);
struct qstr fname2 = QSTR_INIT("m2.mp4", 6);
struct qstr fname3 = QSTR_INIT("m3.mp4", 6);
struct qstr fpath1 = QSTR_INIT("/tagfs/files/m1.mp4", 19);
struct qstr fpath2 = QSTR_INIT("/tagfs/files/m2.mp4", 19);
struct qstr fpath3 = QSTR_INIT("/tagfs/files/m3.mp4", 19);



struct qstr kEmptyQStr = QSTR_INIT("", 0);


int tagfs_init_storage(void) {
  kEmptyQStr.hash = full_name_hash(NULL, kEmptyQStr.name, kEmptyQStr.len);

  tag1.hash = full_name_hash(NULL, tag1.name, tag1.len);
  tag2.hash = full_name_hash(NULL, tag2.name, tag2.len);
  tag3.hash = full_name_hash(NULL, tag3.name, tag3.len);

  fname1.hash = full_name_hash(NULL, fname1.name, fname1.len);
  fname2.hash = full_name_hash(NULL, fname2.name, fname2.len);
  fname3.hash = full_name_hash(NULL, fname3.name, fname3.len);

  return 0;
}


void tagfs_sync_storage(void) {
}


ssize_t tagfs_get_tags_amount(void) {
  return 3;
}


struct qstr tagfs_get_tag_by_index(ssize_t index) {
  switch (index) {
    case 0: return tag1; break;
    case 1: return tag2; break;
    case 2: return tag3; break;
  }
  return kEmptyQStr;
}


ssize_t tagfs_get_files_amount(void) {
  return 3;
}

const struct qstr* tagfs_get_fname_by_index(ssize_t index) {
  switch (index) {
    case 0: return &fname1; break;
    case 1: return &fname2; break;
    case 2: return &fname3; break;
  }
  return NULL;
}
