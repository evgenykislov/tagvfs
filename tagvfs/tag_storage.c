#include "tag_storage.h"

#include <linux/slab.h>


struct qstr tag1 = QSTR_INIT("Классика", 8);
struct qstr tag2 = QSTR_INIT("Винтаж", 6);
struct qstr tag3 = QSTR_INIT("Модерн", 6);
struct qstr fname1 = QSTR_INIT("m1.mp4", 6);
struct qstr fname2 = QSTR_INIT("m2.mp4", 6);
struct qstr fname3 = QSTR_INIT("m3.mp4", 6);
struct qstr fpath1 = QSTR_INIT("/tagfs/files/m1.mp4", 19);
struct qstr fpath2 = QSTR_INIT("/tagfs/files/m2.mp4", 19);
struct qstr fpath3 = QSTR_INIT("/tagfs/files/m3.mp4", 19);

struct qstr fname4 = QSTR_INIT(NULL, 0);
struct qstr fpath4 = QSTR_INIT(NULL, 0);


struct qstr kSpecNameAllFiles = QSTR_INIT("all-files", 9);
struct qstr kSpecNameFilesWOTags = QSTR_INIT("files-wo-tags", 13);
struct qstr kSpecNameTags = QSTR_INIT("tags", 4);
struct qstr kSpecNameControl = QSTR_INIT("control", 7);


struct qstr kNullQstr = QSTR_INIT(NULL, 0);
struct qstr kEmptyQStr = QSTR_INIT("", 0);

const size_t kNotFoundIno = (size_t)(-1);


/* TODO stub function */
const struct qstr get_fpath_by_index(ssize_t index) {
  switch (index) {
    case 0: return fpath1; break;
    case 1: return fpath2; break;
    case 2: return fpath3; break;
    case 3: return fpath4; break;
  }
  return kNullQstr;
}



/*! Сравнивает две строки. Возвращает 0 в случае равества */
static int compare_qstr(const struct qstr n1, const struct qstr n2) {
  if (n1.len != n2.len) {
    return 1;
  }
  return !!memcmp(n1.name, n2.name, n1.len);
}

int tagfs_init_storage(Storage* stor, const char* file_storage) {
  *stor = kzalloc(5, GFP_KERNEL);

  kEmptyQStr.hash = full_name_hash(NULL, kEmptyQStr.name, kEmptyQStr.len);

  tag1.hash = full_name_hash(NULL, tag1.name, tag1.len);
  tag2.hash = full_name_hash(NULL, tag2.name, tag2.len);
  tag3.hash = full_name_hash(NULL, tag3.name, tag3.len);

  fname1.hash = full_name_hash(NULL, fname1.name, fname1.len);
  fname2.hash = full_name_hash(NULL, fname2.name, fname2.len);
  fname3.hash = full_name_hash(NULL, fname3.name, fname3.len);

  return 0;
}

void tagfs_release_storage(Storage* stor) {
  kfree(*stor);
  *stor = NULL;
}


void tagfs_sync_storage(Storage stor) {
}


ssize_t tagfs_get_tags_amount(Storage stor) {
  return 3;
}


struct qstr tagfs_get_tag_by_index(Storage stor, ssize_t index) {
  switch (index) {
    case 0: return tag1; break;
    case 1: return tag2; break;
    case 2: return tag3; break;
  }
  return kEmptyQStr;
}


ssize_t tagfs_get_files_amount(Storage stor) {
  return fname4.name ? 4: 3;
}

const struct qstr tagfs_get_fname_by_index(Storage stor, ssize_t index) {
  switch (index) {
    case 0: return fname1; break;
    case 1: return fname2; break;
    case 2: return fname3; break;
    case 3: if (fname4.name) { return fname4; } break;
  }
  return kNullQstr;
}



struct qstr tagfs_get_special_name(Storage stor, enum FSSpecialName name) {
  switch (name) {
    case kFSSpecialNameAllFiles: return kSpecNameAllFiles; break;
    case kFSSpecialNameFilesWOTags: return kSpecNameFilesWOTags; break;
    case kFSSpecialNameTags: return kSpecNameTags; break;
    case kFSSpecialNameControl: return kSpecNameControl; break;
    case kFSSpecialNameUndefined: return kNullQstr; break;
  }
  return kNullQstr;
}

enum FSSpecialName tagfs_get_special_type(Storage stor, struct qstr name) {
  if (compare_qstr(name, kSpecNameAllFiles) == 0) { return kFSSpecialNameAllFiles; }
  if (compare_qstr(name, kSpecNameFilesWOTags) == 0) { return kFSSpecialNameFilesWOTags; }
  if (compare_qstr(name, kSpecNameTags) == 0) { return kFSSpecialNameTags; }
  if (compare_qstr(name, kSpecNameControl) == 0) { return kFSSpecialNameControl; }
  return kFSSpecialNameUndefined;
}

void tagfs_get_first_name(Storage stor, size_t start_ino,
    const struct TagMask* mask, size_t* found_ino, struct qstr* name) {
  struct qstr rstr;
  rstr = tagfs_get_fname_by_index(stor, start_ino);
  if (rstr.len == 0) {
    *found_ino = kNotFoundIno;
    *name = kNullQstr;
    return;
  }

  *found_ino = start_ino;
  *name = rstr;
}

size_t tagfs_get_ino_of_name(Storage stor, const struct qstr name) {
  if (compare_qstr(name, fname1) == 0) { return 0; }
  if (compare_qstr(name, fname2) == 0) { return 1; }
  if (compare_qstr(name, fname3) == 0) { return 2; }
  if (compare_qstr(name, fname4) == 0) { return 3; }
  return kNotFoundIno;
}


size_t tagfs_get_file_size(Storage stor, size_t ino) {
  struct qstr data;
  data = get_fpath_by_index(ino);
  return data.len;
}

size_t tagfs_get_file_data(Storage stor, size_t ino, size_t pos, size_t len, void* buffer) {
  struct qstr data;
  size_t available;
  data = get_fpath_by_index(ino);
  if (pos >= data.len) { return 0; /* TODO error */}
  if ((pos + len) <= data.len) {
    available = len;
  } else {
    available = data.len - pos;
  }

  memcpy(buffer, data.name + pos, available);
  return available;
}

size_t tagfs_add_new_file(Storage stor, const char* target_name, const struct qstr link_name) {
  void* n;
  fpath4.len = strlen(target_name);
  n = kzalloc(fpath4.len, GFP_KERNEL); // TODO check result
  memcpy(n, target_name, fpath4.len);
  fpath4.name = n;
  fname4.len = link_name.len;
  n = kzalloc(fname4.len, GFP_KERNEL); // TODO check result
  memcpy(n, link_name.name, fname4.len);
  fname4.name = n;
  return 3;
}
