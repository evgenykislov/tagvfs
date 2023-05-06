#include "tag_storage.h"

#include <linux/fs.h>
#include <linux/slab.h>

#include "common.h"

const u32 kMagicWord = 0x34562343;
const u64 kTablesAlignment = 256;

const u16 kDefaultTagRecordSize = 256;
const u16 kDefaultTagRecordAmount = 64;
const u16 kDefaultFileBlockSize = 256;

const size_t kMaxFileBlocks = 1000; //!< Предельное ограничение на очень длинное описание файла (в блоках)

struct FSHeader {
  __le32 magic_word;
  __le32 padding1;
  __le64 tag_table_pos;
  /*0x10*/
  __le64 fileblock_table_pos;
  __le16 tag_record_size;
  __le16 tag_record_amount;
  __le16 fileblock_size;
  __le16 padding2;
  /*0x20*/
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
  __le16 link_name_size; //!< Это название символьной ссылки
  __le32 link_target_size; //!< Это содержимое символьной ссылки (путь к внешнему файлу)
  /* tag field */
  /* link name field */
  /* link target field */
};

struct StorageRaw {
  struct FSHeader header_mem; //!< Копия хедера в памяти для изменения и записи

  u64 tag_table_pos;
  u16 tag_record_size;
  u16 tag_record_amount;
  u16 tag_min_size; //!< Минимальный размер тэговых битов (в байтах)

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

  if (!stor || (*stor)) { return -EINVAL; }

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
  sr->tag_min_size = (sr->tag_record_amount + 63) / 64 * 8;

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
  loff_t wpos = 0;
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
struct qstr fname2 = QSTR_INIT("m2.mp4", 6);
struct qstr fname3 = QSTR_INIT("m3.mp4", 6);
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

/*! Вычитывает инфомрацию о файле по номеру (ino). Для данных выдуляет блок
 памяти.
\param stor описатель хранилища
\param ino номер файла
\param data указатель на возвращаемый буфер с данными. Должно быть *data == NULL
\param data_size указатель на размер данных
\return 0 - если всё хорошо. Иначе отрицательный код ошибки */
int AllocateReadFileData(struct StorageRaw* stor, size_t ino, void** data,
                         size_t* data_size) {
  struct FileBlockHeader* block = NULL;
  loff_t pos;
  size_t rs;
  int res = 0;
  size_t cur_nod;
  size_t prev_nod;
  size_t block_counter;

  if (!stor || !stor->storage_file) { return -EINVAL; }
  if (!data || *data) { return -EINVAL; }
  if (!data_size) { return -EINVAL; }
  if (ino >= stor->fileblock_amount) { return -EINVAL; }
  if (stor->fileblock_size <= sizeof(struct FileBlockHeader)) { return -EINVAL; }

  *data_size = 0;

  block = kmalloc(stor->fileblock_size, GFP_KERNEL);
  if (!block) { return -ENOMEM; }

  prev_nod = ino;
  cur_nod = ino;
  block_counter = 0;
  while (block_counter < kMaxFileBlocks) {
    size_t new_size;
    size_t add_size;
    size_t next_block;

    pos = stor->fileblock_table_pos + cur_nod * stor->fileblock_size;
    rs = kernel_read(stor->storage_file, block, stor->fileblock_size, &pos);
    if (rs != stor->fileblock_size) {
      res = -EFAULT;
      goto err_allmem;
    }
    if (block->prev_block_index != prev_nod) {
      res = -ESPIPE;
      goto err_allmem;
    }

    add_size = stor->fileblock_size - sizeof(struct FileBlockHeader);
    new_size = *data_size + add_size;
    *data = krealloc(*data, new_size, GFP_KERNEL);
    if (!*data) {
      res = -ENOMEM;
      goto err_allmem;
    }

    memcpy(*data + *data_size, ((u8*)block) + sizeof(struct FileBlockHeader),
        add_size);
    *data_size = new_size;

    // Найдём следующий блок
    next_block = block->next_block_index;
    if (next_block == cur_nod) {
      // Это был последний блок
      break;
    }
    if (next_block == (size_t)(-1)) {
      res = -ESPIPE;
      goto err_allmem;
    }

    prev_nod = cur_nod;
    cur_nod = next_block;
  }

  if (block_counter >= kMaxFileBlocks) {
    res = -EFBIG;
    goto err_allmem;
  }

  return 0;

  // ----------
err_allmem:
  if (*data) {
    kfree(*data);
    *data = NULL;
    *data_size = 0;
  }
  kfree(block);
  return res;
}


/* ??? */
void FreeFileData(void** data, size_t* data_size) {
  if (data) {
    kfree(*data);
    *data = NULL;
  }
  if (data_size) {
    *data_size = 0;
  }
}

/* ??? */
int GetFileInfo(struct StorageRaw* sr, size_t ino, void* tag,
    struct qstr* link_name, struct qstr* link_target) {
  void* data = NULL;
  size_t data_size = 0;
  int res;
  struct FileHeader* fh;
  size_t postag, namepos, targetpos;
  size_t taglen, namelen, targetlen;

  if (!sr) { return -EINVAL; }

  pr_info("TODO p0\n");

  res = AllocateReadFileData(sr, ino, &data, &data_size);
  if (res) {
    pr_info("TODO p0 - err: %d\n", (int)res);
    return res;
  }

  pr_info("TODO p1\n");

  fh = (struct FileHeader*)(data);
  postag = sizeof(struct FileHeader);
  taglen = le16_to_cpu(fh->tags_field_size);
  if (tag) {
    // TODO implement
  }

  pr_info("TODO p2\n");

  namepos = postag + taglen;
  namelen = le16_to_cpu(fh->link_name_size);
  if (link_name) {
    *link_name = alloc_qstr_from_str(data + namepos, namelen);
  }

  targetpos = namepos + namelen;
  targetlen = le32_to_cpu(fh->link_target_size);
  if (link_target) {
    *link_target = alloc_qstr_from_str(data + targetpos, targetlen);
  }

  FreeFileData(&data, &data_size);

  return 0;
}

/* Возвращает количество записанных байтов */
size_t AddFileBlock(struct StorageRaw* sr, size_t fb_index, size_t prev_fb,
    size_t next_fb, void* data, size_t len) {
  // TODO IMPROVE PERFORMANCE
  size_t avail;
  size_t zero_tail;
  struct FileBlockHeader h;
  loff_t pos;
  size_t ws;
  size_t max_bl;

  if (sr->fileblock_size <= sizeof(struct FileBlockHeader)) {
    // TODO inform about logic error
    return 0;
  }

  avail = sr->fileblock_size - sizeof(struct FileBlockHeader);
  if (avail > len) {
    avail = len;
  }
  zero_tail = sr->fileblock_size - sizeof(struct FileBlockHeader) - avail;

  h.prev_block_index = prev_fb;
  h.next_block_index = next_fb;
  pos = sr->fileblock_table_pos + sr->fileblock_size * fb_index;
  ws = kernel_write(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) {
    // TODO EROIRO
    return 0;
  }
  ws = kernel_write(sr->storage_file, data, avail, &pos);
  if (ws != avail) {
    // TODO EROIRO
    return 0;
  }

  if (zero_tail > 0) {
    void* zero_block = kzalloc(zero_tail, GFP_KERNEL);
    ws = kernel_write(sr->storage_file, zero_block, zero_tail, &pos);
    kfree(zero_block);
    if (ws != zero_tail) {
      // TODO EROROROOER
      return 0;
    }

  }


  max_bl = fb_index + 1;
  if (sr->fileblock_amount < max_bl) {
    loff_t zeropos = 0;
    sr->fileblock_amount = max_bl; // TODO LOCK-LOCK
    sr->header_mem.fileblock_amount = cpu_to_le64(sr->fileblock_amount);
    ws = kernel_write(sr->storage_file, &sr->header_mem, sizeof(struct FSHeader),
        &zeropos);
    if (ws != sizeof(struct FSHeader)) {
      // TODO ERROR
      return 0;
    }
  }

  return avail;
}

/* Закрыть цепочку файловых блоков */
int CapFileBlock(struct StorageRaw* sr, size_t fb_index) {
  struct FileBlockHeader h;
  loff_t block_pos;
  loff_t pos;
  size_t ws;

  block_pos = sr->fileblock_table_pos + sr->fileblock_size * fb_index;
  pos = block_pos;
  ws = kernel_read(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) { return -EFAULT; }
  h.next_block_index = fb_index;
  pos = block_pos;
  ws = kernel_write(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) { return -EFAULT; }
  return 0;
}

int IncFileAmount(struct StorageRaw* sr) {
  loff_t zeropos = 0;
  size_t ws;

  ++sr->file_amount; // TODO LOCK-LOCK
  sr->header_mem.file_amount = cpu_to_le64(sr->file_amount);
  ws = kernel_write(sr->storage_file, &sr->header_mem, sizeof(struct FSHeader),
      &zeropos);
  if (ws != sizeof(struct FSHeader)) {
    return -EFAULT;
  }
  return 0;
}

/* name - это содержимое линки, целевой файл, link - это имя символьной ссылки */
size_t AddFile(struct StorageRaw* sr, const char* link_name, size_t link_name_len,
    const char* target_link, size_t target_link_len) {
  size_t file_info_size;
  void* file_info;
  size_t res = kNotFoundIno;
  struct FileHeader* fh;
  size_t pos;
  size_t fb_cur, fb_prev, fb_next;
  void* chunk;
  size_t chunk_tail;
  size_t ino = kNotFoundIno;


  file_info_size = sizeof(struct FileHeader) + sr->tag_min_size + link_name_len + target_link_len;
  file_info = kzalloc(file_info_size, GFP_KERNEL);
  if (!file_info) {
    goto err_nomem;
  }

  fh = (struct FileHeader*)(file_info);
  fh->tags_field_size = cpu_to_le16(sr->tag_min_size);
  fh->link_name_size = cpu_to_le16(link_name_len);
  fh->link_target_size = cpu_to_le16(target_link_len);
  pos = sizeof(struct FileHeader) + sr->tag_min_size;
  memcpy(file_info + pos, link_name, link_name_len);
  pos += link_name_len;
  memcpy(file_info + pos, target_link, target_link_len);

  fb_cur = sr->fileblock_amount;
  ino = sr->fileblock_amount;
  fb_prev = fb_cur;
  chunk = file_info;
  chunk_tail = file_info_size;

  while (chunk_tail > 0) {
    size_t red;
    fb_next = fb_cur + 1;
    red = AddFileBlock(sr, fb_cur, fb_prev, fb_next, chunk, chunk_tail);
    if (red == 0 || red > chunk_tail) {
      // TODO ERORR
      goto err;
    }
    chunk += red;
    chunk_tail -= red;
    fb_prev = fb_cur;
    fb_cur = fb_next;
  }
  if (CapFileBlock(sr, fb_prev)) { // fb_prev has value of last written block
    // TODO EROASDFASDF
    goto err;
  }
  if (IncFileAmount(sr)) {
    // TODO ERRREDWR
    goto err;
  }

  res = ino;
  // ---------------------
err:
  kfree(file_info);
err_nomem:
  return res;
}




// TODO CHECK USELESS
/* Возвращает ссылку для файла по его номеру (ino). Если файла с таким номером
нет, то возвращается пустая (null) строка.
\param ino номер (ino) файла
\return путь-ссылка для файла. Строку должен удалить получать результата */
struct qstr get_fpath_by_ino(struct StorageRaw* sr, size_t ino) {
  struct qstr res;

  if (GetFileInfo(sr, ino, NULL, NULL, &res)) {
    return kNullQstr;
  }
  return res;
}


int tagfs_init_storage(Storage* stor, const char* file_storage) {
  int err;

  err = OpenTagFS(stor, file_storage);
  if (err < 0) {
    err = CreateDefaultStorageFile(file_storage);
    if (err < 0) {
      return err;
    }

    err = OpenTagFS(stor, file_storage);
    if (err < 0) {
      return err;
    }
  }
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
  struct StorageRaw* sr;

  if (!stor) { return 0; }
  sr = (struct StorageRaw*)(stor);

  return sr->file_amount;
}

struct qstr tagfs_get_fname_by_ino(Storage stor, size_t ino) {
  struct StorageRaw* sr;
  struct qstr res;

  if (!stor) { return kNullQstr; }
  sr = (struct StorageRaw*)(stor);
  if (GetFileInfo(sr, ino, NULL, &res, NULL)) { return kNullQstr; }
  return res;
}


struct qstr tagfs_get_special_name(Storage stor, enum FSSpecialName name) {
  struct qstr fixn = kNullQstr;
  switch (name) {
    case kFSSpecialNameAllFiles:    fixn = kSpecNameAllFiles; break;
    case kFSSpecialNameFilesWOTags: fixn = kSpecNameFilesWOTags; break;
    case kFSSpecialNameTags:        fixn = kSpecNameTags; break;
    case kFSSpecialNameControl:     fixn = kSpecNameControl; break;
    case kFSSpecialNameUndefined:   fixn = kNullQstr; break;
  }

  // Создание копии строки сделано для единообразия API. В целом это,
  // конечно, просадка производительности
  return alloc_qstr_from_qstr(fixn);
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
  // TODO NEED TO IMPROVE PERFORMANCE
  struct StorageRaw* sr;
  size_t i;

  if (!stor) { goto err; }
  sr = (struct StorageRaw*)(stor);

  for (i = start_ino; i < sr->fileblock_amount; ++i) {
    struct qstr res;

    if (GetFileInfo(sr, i, NULL, &res, NULL)) {
      continue;
    }

    if (name) {
      *name = res;
    } else {
      free_qstr(&res);
    }
    *found_ino = i;
    return;
  }

err:
  *found_ino = kNotFoundIno;
  if (name) { *name = kNullQstr; }
}

size_t tagfs_get_ino_of_name(Storage stor, const struct qstr name) {
  // TODO NEED TO IMPROVE PERFORMANCE
  struct StorageRaw* sr;
  size_t i;

  if (!stor) { return kNotFoundIno; }
  sr = (struct StorageRaw*)(stor);
  for (i = 0; i < sr->fileblock_amount; ++i) {
    struct qstr res;
    int cmp;
    if (GetFileInfo(sr, i, NULL, &res, NULL)) {
      continue;
    }
    cmp = compare_qstr(res, name);
    free_qstr(&res);
    if (cmp == 0) {
      return i;
    }
  }
  return kNotFoundIno;
}


struct qstr tagfs_get_file_link(Storage stor, size_t ino) {
  struct StorageRaw* sr;

  if (!stor) { return kNullQstr; }
  sr = (struct StorageRaw*)(stor);
  return get_fpath_by_ino(sr, ino);
}


size_t tagfs_add_new_file(Storage stor, const char* target_name,
    const struct qstr link_name) {
  struct StorageRaw* sr;

  if (!stor) { return kNotFoundIno; }
  sr = (struct StorageRaw*)(stor);
  return AddFile(sr, link_name.name, link_name.len, target_name,
      strlen(target_name));
}
