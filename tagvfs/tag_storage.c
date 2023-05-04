#include "tag_storage.h"

#include <linux/fs.h>
#include <linux/slab.h>

#include "common.h"

const u32 kMagicWord = 0x34562343;
const u64 kTablesAlignment = 256;

const u16 kDefaultTagRecordSize = 256;
const u16 kDefaultTagRecordAmount = 64;
const u16 kDefaultFileBlockSize = 256;

struct FSHeader {
  __le32 magic_word;
  __le32 padding1;
  __le64 tag_table_pos;
  /*10*/
  __le64 fileblock_table_pos;
  __le16 tag_record_size;
  __le16 tag_record_amount;
  __le16 fileblock_size;
  __le16 padding2;
  /*20*/
  __le64 fileblock_amount;
  __le64 file_amount;
};


struct TagHeader {
  __le16 tag_flags;
  __le16 tag_name_size;
  /* name after header */
};

struct FileBlockHeader {
  __le64 prev_block_index;
  __le64 next_block_index;
  /* file data after header */
};

struct FileHeader {
  __le16 tags_field_size;
  __le16 file_name_size;
  __le32 link_name_size;
  /* tag field */
  /* name field */
  /* link field */
};

struct StorageRaw {
  struct FSHeader header_mem; //!< Копия хедера в памяти для изменения и записи

  u64 tag_table_pos;
  u16 tag_record_size;
  u16 tag_record_amount;

  u64 fileblock_table_pos;
  u16 fileblock_size;
  u64 fileblock_amount;
  u64 file_amount;

  struct file* storage_file;
};

/*! Открывает файл-хранилище и инициализирует экземпляр stor
\param stor инициализируемое хранилище
\param file_storage имя файла-хранилища
\return 0 - открытие успешно. Или отрицательный код ошибки */
int OpenTagFS(Storage* stor, const char* file_storage) {
  struct file* f = NULL;
  loff_t rpos;
  ssize_t rs;
  struct StorageRaw* sr;
  int res = 0;

  if (!stor || (*stor)) {
    pr_err(kModuleLogName "Logic error: 'stor'-open wrong initialization at %s:%i\n", __FILE__, __LINE__);
    return -EINVAL;
  }

  *stor = kzalloc(sizeof(struct StorageRaw), GFP_KERNEL);
  if (!(*stor)) {
    return -ENOMEM;
  }
  sr = (struct StorageRaw*)(*stor);

  f = filp_open(file_storage, O_RDWR, 0);
  if (IS_ERR(f)) {
    res = PTR_ERR(f);
    goto err_aa;
  }

  rpos = 0;
  rs = kernel_read(f, &(sr->header_mem), sizeof(struct FSHeader), &rpos);
  if (rs != sizeof(struct FSHeader)) {
    goto err_ao;
  }

  sr->tag_table_pos = le64_to_cpu(sr->header_mem.tag_table_pos);
  sr->tag_record_size = le16_to_cpu(sr->header_mem.tag_record_size);
  sr->tag_record_amount = le16_to_cpu(sr->header_mem.tag_record_amount);

  sr->fileblock_table_pos = le64_to_cpu(sr->header_mem.fileblock_table_pos);
  sr->fileblock_size = le16_to_cpu(sr->header_mem.fileblock_size);
  sr->fileblock_amount = le64_to_cpu(sr->header_mem.fileblock_amount);
  sr->file_amount = le64_to_cpu(sr->header_mem.file_amount);

  sr->storage_file = f;

  return 0;
  // --------------
err_ao:
  filp_close(f, NULL);
err_aa:
  kfree(*stor);
  *stor = NULL;

  return res; // File doesn't exist
}

/*! TODO ??? */
int CloseTagFS(Storage* stor) {
  struct StorageRaw* sr;

  if (!stor || !(*stor)) {
    pr_err(kModuleLogName "Logic error: 'stor'-close wrong initialization at %s:%i\n", __FILE__, __LINE__);
    return -EINVAL;
  }

  sr = (struct StorageRaw*)(*stor);
  filp_close(sr->storage_file, NULL);
  kfree(sr);
  *stor = NULL;
  return 0;
}


/*! Создать файл-хранилище с дефалтовыми настройками. Функция
создаёт новый файл (если файл уже существует, то вернётся ошибка),
заполняет форматки в файле и закрывает заполненный файл.
\param file_storage имя создаваемого файла
\return 0 - создание успешно, Или отрицательный код ошибки */
int CreateDefaultStorageFile(const char* file_storage) {
  struct file* f = NULL;
  struct FSHeader h;
  u64 tag_pos = 0;
  u64 file_pos = 0;
  ssize_t ws;
  loff_t wpos;
  int res;

  f = filp_open(file_storage, O_RDWR | O_CREAT | O_EXCL, 0666 /* TODO CHANGE RIGHTS S_IRUSR | S_IWUSR */);
  if (IS_ERR(f)) {
    return PTR_ERR(f);
  }

  // Fill and write header
  tag_pos = (sizeof(struct FSHeader) + kTablesAlignment - 1) / kTablesAlignment * kTablesAlignment;
  file_pos = (tag_pos + kDefaultTagRecordSize * kDefaultTagRecordAmount + kTablesAlignment - 1) / kTablesAlignment * kTablesAlignment;
  h.magic_word = cpu_to_le32(kMagicWord);
  h.padding1 = 0;
  h.tag_table_pos = cpu_to_le64(tag_pos);
  h.fileblock_table_pos = cpu_to_le64(file_pos);
  h.tag_record_size = cpu_to_le16(kDefaultTagRecordSize);
  h.tag_record_amount = cpu_to_le16(kDefaultTagRecordAmount);
  h.fileblock_size = cpu_to_le16(kDefaultFileBlockSize);
  h.padding2 = 0;
  h.fileblock_amount = cpu_to_le64(0);
  h.file_amount = cpu_to_le64(0);
  ws = kernel_write(f, &h, sizeof(h), &wpos);

  res = filp_close(f, NULL);
  return res;
}




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
  int err;

  err = OpenTagFS(stor, file_storage);
  if (err < 0) {
    err = CreateDefaultStorageFile(file_storage);
    if (err < 0) {
      return err;
    }

  }


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
  CloseTagFS(stor);
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
