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


struct qstr kSpecNameAllFiles = QSTR_INIT("all-files", 9);
struct qstr kSpecNameFilesWOTags = QSTR_INIT("files-wo-tags", 13);
struct qstr kSpecNameTags = QSTR_INIT("tags", 4);
struct qstr kSpecNameControl = QSTR_INIT("control", 7);


struct qstr kNullQstr = QSTR_INIT(NULL, 0);
struct qstr kEmptyQStr = QSTR_INIT("", 0);

const size_t kNotFoundIno = (size_t)(-1);

/*! Сравнивает две строки. Возвращает 0 в случае равества */
static int compare_qstr(const struct qstr n1, const struct qstr n2) {
  if (n1.len != n2.len) {
    return 1;
  }
  return !!memcmp(n1.name, n2.name, n1.len);
}

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

const struct qstr tagfs_get_fname_by_index(ssize_t index) {
  switch (index) {
    case 0: return fname1; break;
    case 1: return fname2; break;
    case 2: return fname3; break;
  }
  return kNullQstr;
}


struct qstr tagfs_get_special_name(enum FSSpecialName name) {
  switch (name) {
    case kFSSpecialNameAllFiles: return kSpecNameAllFiles; break;
    case kFSSpecialNameFilesWOTags: return kSpecNameFilesWOTags; break;
    case kFSSpecialNameTags: return kSpecNameTags; break;
    case kFSSpecialNameControl: return kSpecNameControl; break;
    case kFSSpecialNameUndefined: return kNullQstr; break;
  }
  return kNullQstr;
}

enum FSSpecialName tagfs_get_special_type(struct qstr name) {
  if (compare_qstr(name, kSpecNameAllFiles) == 0) { return kFSSpecialNameAllFiles; }
  if (compare_qstr(name, kSpecNameFilesWOTags) == 0) { return kFSSpecialNameFilesWOTags; }
  if (compare_qstr(name, kSpecNameTags) == 0) { return kFSSpecialNameTags; }
  if (compare_qstr(name, kSpecNameControl) == 0) { return kFSSpecialNameControl; }
  return kFSSpecialNameUndefined;
}

void tagfs_get_first_name(size_t start_ino, const struct TagMask* mask,
    size_t* found_ino, struct qstr* name) {
  struct qstr rstr;
  rstr = tagfs_get_fname_by_index(start_ino);
  if (rstr.len == 0) {
    *found_ino = kNotFoundIno;
    *name = kNullQstr;
    return;
  }

  *found_ino = start_ino;
  *name = rstr;
}

size_t tagfs_get_ino_of_name(const struct qstr name) {
  if (compare_qstr(name, fname1) == 0) { return 0; }
  if (compare_qstr(name, fname2) == 0) { return 1; }
  if (compare_qstr(name, fname3) == 0) { return 2; }
  return kNotFoundIno;
}
