#include "tag_storage.h"

#include <linux/fs.h>
#include <linux/slab.h>

#include "common.h"

const u32 kMagicWord = 0x34562343;
const u64 kTablesAlignment = 256;

const u16 kDefaultTagRecordSize = 256;
const u16 kDefaultTagRecordMaxAmount = 64;
const u16 kDefaultFileBlockSize = 256;

const size_t kMaxFileBlocks = 1000; //!< Предельное ограничение на очень длинное описание файла (в блоках)

struct FSHeader {
  __le32 magic_word;
  __le32 padding1; //!< Выравнивание. Заполняется нулями
  __le64 tag_table_pos; //!< Позиция (абсолютная) начала таблицы с тэгами. Может быть больше размера файла
  /*0x10*/
  __le64 fileblock_table_pos; //!< Позиция (абсолютная) начала таблицы с файловыми блоками. Может быть больше размера файла.
  __le16 tag_record_size;
  __le16 tag_record_max_amount; //!< Предельное количество записей по тегам
  __le16 tag_filled_records; //!< Текущее количество заполненных записей тэгов. Учитывает удалённые тэги. Фактически позволяет определить позицию добавления нового тэга
  __le16 fileblock_size; //!< Размер файлового блока
  /*0x20*/
  __le64 fileblock_amount; //!< Общее количество записанных файловых блоков (т.е. тех, что можно прочитать). Фактически определяет размер файла
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
  u16 tag_record_max_amount;
  u16 tag_filled_records;
  u16 tag_mask_byte_size; //!< Размер тэговой битовой маски (в байтах)
  u16 active_tag_amount; //!< Вычисляемое: Количество активных (не-удалённых) тэгов TODO LOCK

  u64 fileblock_table_pos;
  u16 fileblock_size;
  u64 fileblock_amount;
  u64 file_amount; // TODO CHECK USAGE

  size_t min_fileblock_for_seek_empty; // Номер блока, с которого можно начинать поиск свободного блока (для ускорения файловых операций)

  struct file* storage_file;
};

extern void CalculateActiveTagAmount(struct StorageRaw* sr);

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
  sr->tag_record_max_amount = le16_to_cpu(sr->header_mem.tag_record_max_amount);
  sr->tag_filled_records = le16_to_cpu(sr->header_mem.tag_filled_records);
  sr->active_tag_amount = (u16)(-1); // TODO VALUE-VALUE-????
  sr->tag_mask_byte_size = tagmask_get_byte_len(sr->tag_record_max_amount);

  sr->fileblock_table_pos = le64_to_cpu(sr->header_mem.fileblock_table_pos);
  sr->fileblock_size = le16_to_cpu(sr->header_mem.fileblock_size);
  sr->fileblock_amount = le64_to_cpu(sr->header_mem.fileblock_amount);

  sr->min_fileblock_for_seek_empty = 0;

  sr->storage_file = f;

  CalculateActiveTagAmount(sr);

  return 0;
  // --------------
err_ao:
  filp_close(f, NULL);
err_aa:
  kfree(*stor);
  *stor = NULL;

  return res; // File doesn't exist
}

/*! Закрывает хранилище файловой системы. Освобождает все ресурсы.
Обнуляет указатель на хранилище
\param stor указатель на данные хранилища
\return 0 - закрытие без ошибок; Иначе - отрицательный код ошибки */
int CloseTagFS(Storage* stor) {
  struct StorageRaw* sr;

  if (!stor || !(*stor)) { return -EINVAL; }
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
  file_pos = (tag_pos + kDefaultTagRecordSize * kDefaultTagRecordMaxAmount + kTablesAlignment - 1) / kTablesAlignment * kTablesAlignment;
  h.magic_word = cpu_to_le32(kMagicWord);
  h.padding1 = 0;
  h.tag_table_pos = cpu_to_le64(tag_pos);
  h.fileblock_table_pos = cpu_to_le64(file_pos);
  h.tag_record_size = cpu_to_le16(kDefaultTagRecordSize);
  h.tag_record_max_amount = cpu_to_le16(kDefaultTagRecordMaxAmount);
  h.tag_filled_records = cpu_to_le16(0);
  h.fileblock_size = cpu_to_le16(kDefaultFileBlockSize);
  h.fileblock_amount = cpu_to_le64(0);
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

/* Проверяет тэг на валидность (не удалён). Это быстрее, чем вычитывать весь тэг
\param tag - номер тэга
\return признак валидности. true - валидный (активный) */
bool CheckTagIsActive(struct StorageRaw* sr, size_t tag) {
  struct TagHeader th;
  loff_t pos;
  size_t rs;

  BUG_ON(!sr);
  BUG_ON(!sr->storage_file);

  pos = sr->tag_table_pos + tag * sr->tag_record_size;
  rs = kernel_read(sr->storage_file, &th, sizeof(struct TagHeader), &pos);
  if (rs != sizeof(struct TagHeader)) {
    // taginfo is out of file
    return false;
  }

  return th.tag_flags != 0 ? 1: 0;
}


/*! Посчитаем и заполним количество активных тэгов */
void CalculateActiveTagAmount(struct StorageRaw* sr) {
  size_t i;
  size_t amount = 0;

  for (i = 0; i < sr->tag_filled_records; ++i) {
    if (!CheckTagIsActive(sr, i)) { continue; }
    // Has found real/active tag
    ++amount;
  }
  sr->active_tag_amount = amount;
}


int ReadTagName(struct StorageRaw* sr, size_t tag, struct qstr* name) {
  void* tag_info;
  struct TagHeader* th;
  loff_t pos;
  int res = 0;
  size_t rs;

  if (!name) { return -EINVAL; }
  if (name->name || name->len) { return -EINVAL; }
  if (!sr || !sr->storage_file) { return -EINVAL; }

  tag_info = kzalloc(sr->tag_record_size, GFP_KERNEL);
  if (!tag_info) { return -ENOMEM; }

  pos = sr->tag_table_pos + tag * sr->tag_record_size;
  rs = kernel_read(sr->storage_file, tag_info, sr->tag_record_size, &pos);
  if (rs != sr->tag_record_size) {
    res = -EFAULT;
    goto err;
  }
  th = (struct TagHeader*)(tag_info);
  if ((th->tag_name_size + sizeof(struct TagHeader)) > sr->tag_record_size) {
    res = -EFAULT;
    goto err;
  }
  *name = alloc_qstr_from_str(tag_info + sizeof(struct TagHeader),
      th->tag_name_size);
  // -----------------
err:
  kfree(tag_info);
  return res;
}


/* Проверяет файл на валидность (не удалён). Это быстрее, чем вычитывать весь файл
\param file - номер файла
\return 0 - невалидный (удалённый), 1 - валидный (активный), <0 - ошибка */
bool CheckFileIsActive(struct StorageRaw* sr, size_t file) {
  struct FileBlockHeader bh;
  loff_t pos;
  size_t rs;

  BUG_ON(!sr);
  BUG_ON(!sr->storage_file);

  if (file >= sr->fileblock_amount) {
    return false;
  }

  pos = sr->fileblock_table_pos + file * sr->fileblock_size;
  rs = kernel_read(sr->storage_file, &bh, sizeof(struct FileBlockHeader), &pos);
  if (rs != sizeof(struct FileBlockHeader)) {
    return false;
  }

  return bh.prev_block_index == file;
}



/*! Вычитывает информацию о файле по номеру (ino). Для данных выделяется блок
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
int GetFileInfo(struct StorageRaw* sr, size_t ino, struct TagMask* tag_mask,
    struct qstr* link_name, struct qstr* link_target) {
  void* data = NULL;
  size_t data_size = 0;
  int res;
  struct FileHeader* fh;
  size_t tagpos, namepos, targetpos;
  size_t taglen, namelen, targetlen;

  if (!sr) { return -EINVAL; }

  res = AllocateReadFileData(sr, ino, &data, &data_size);
  if (res) {
    return res;
  }

  fh = (struct FileHeader*)(data);
  tagpos = sizeof(struct FileHeader);
  taglen = le16_to_cpu(fh->tags_field_size);
  if (tag_mask) {
    *tag_mask = tagmask_init_zero(sr->tag_record_max_amount);
    tagmask_fill_from_buffer(*tag_mask, data + tagpos, taglen);
  }

  namepos = tagpos + taglen;
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


/*! Очищает файловый блок - маркирует как свободный. Также возвращает номер
блока, который должен продолжать цепочку файловых блоков
\param fb_index индекс удаляемого файлового блока
\param next_fb индекс файлового блока, который следующий в цепочке. Параметр может быть NULL
\return код ошибки. 0 - если ошибок нет */
int ClearFileBlock(struct StorageRaw* sr, size_t fb_index, size_t* next_fb) {
  struct FileBlockHeader h;
  loff_t block_pos;
  loff_t pos;
  size_t ws;

  block_pos = sr->fileblock_table_pos + sr->fileblock_size * fb_index;
  pos = block_pos;
  ws = kernel_read(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) { return -EFAULT; }

  if (next_fb) { *next_fb = h.next_block_index; }

  h.prev_block_index = (size_t)(-1);
  h.next_block_index = (size_t)(-1);
  pos = block_pos;
  ws = kernel_write(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) { return -EFAULT; }

  if (sr->min_fileblock_for_seek_empty > fb_index) {
    sr->min_fileblock_for_seek_empty = fb_index;
  }
  return 0;
}


/* Переписать (обновить) данные в файловом описателе. Другие данные остаются
на месте, ничего не сдвигается. Если место записи выходит за границу существующей
цепочки блоков, то запись прерывается. Функция может вызываться рекурсивно.
\param blockino номер блока, в котором обновляются данные
\param data данные для записи
\param data_size размер данных для обновления
\param data_pos позиция в файловом описателе, где обновлять данные
\param nest_counter счётчик вложенности вызовов. В начальном вызове должен быть 0
\return размер обновлённых данных */
size_t UpdateDataIntoBlockChain(struct StorageRaw* sr, size_t blockino,
    void* data, size_t data_size, size_t data_pos, size_t nest_counter) {
  struct FileBlockHeader h;
  loff_t block_pos;
  loff_t pos;
  size_t ws;
  size_t max_data = sr->fileblock_size - sizeof(h);

  pr_info("TODO update data in %zu block\n", blockino);

  if (nest_counter > kMaxFileBlocks) { return 0; }

  block_pos = sr->fileblock_table_pos + sr->fileblock_size * blockino;
  pr_info("TODO block begin pos %zx\n", (size_t)block_pos);

  pos = block_pos;
  ws = kernel_read(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) { return 0; }

  if (data_pos < max_data) {
    size_t tail = max_data - data_pos;
    if (tail > data_size) {
      tail = data_size;
    }
    pos = block_pos + sizeof(h) + data_pos;
    ws = kernel_write(sr->storage_file, data, tail, &pos);
    if (ws != tail) { return ws; }
    if (ws == data_size) { return ws; }
    if (h.next_block_index == blockino) {
      return ws;
    }

    return ws + UpdateDataIntoBlockChain(sr, h.next_block_index, data + ws,
        data_size - ws, 0, nest_counter + 1);
  }

  // Нет записи в текущем блоке. Идём в следующий
  if (h.next_block_index == blockino) { return 0; }
  return UpdateDataIntoBlockChain(sr, h.next_block_index, data, data_size,
      data_pos - max_data, nest_counter + 1);
}

/* ??? */
int IncTagFilledAmount(struct StorageRaw* sr) {
  loff_t zeropos = 0;
  size_t ws;

  ++sr->tag_filled_records; // TODO LOCK-LOCK
  sr->header_mem.tag_filled_records = cpu_to_le16(sr->tag_filled_records);
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


  file_info_size = sizeof(struct FileHeader) + sr->tag_mask_byte_size + link_name_len + target_link_len;
  file_info = kzalloc(file_info_size, GFP_KERNEL);
  if (!file_info) {
    goto err_nomem;
  }

  fh = (struct FileHeader*)(file_info);
  fh->tags_field_size = cpu_to_le16(sr->tag_mask_byte_size);
  fh->link_name_size = cpu_to_le16(link_name_len);
  fh->link_target_size = cpu_to_le16(target_link_len);
  pos = sizeof(struct FileHeader) + sr->tag_mask_byte_size;
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

  res = ino;
  // ---------------------
err:
  kfree(file_info);
err_nomem:
  return res;
}

/*! Удалить запись о файле из хранилища
\param fileino номер файла
\return код ошибки */
int DelFile(struct StorageRaw* sr, size_t fileino) {
  size_t fi = fileino;
  int res = 0;

  while (true) {
    size_t fn;

    res = ClearFileBlock(sr, fi, &fn);
    if (res) { return res; }
    fi = fn;
  }
  return res;
}


size_t AddTag(struct StorageRaw* sr, const char* name, size_t name_len) {
  void* tag;
  struct TagHeader* th;
  loff_t pos;
  size_t ws;
  int res = 0;

  if (sr->tag_filled_records >= sr->tag_record_max_amount) { return -ENOMEM; }

  tag = kzalloc(sr->tag_record_size, GFP_KERNEL);
  if (!tag) { return -ENOMEM; }
  th = (struct TagHeader*)(tag);
  th->tag_flags = cpu_to_le16(0x01);
  th->tag_name_size = sr->tag_record_size - sizeof(struct TagHeader);
  if (th->tag_name_size > name_len) {
    th->tag_name_size = name_len;
  }
  memcpy(tag + sizeof(struct TagHeader), name, th->tag_name_size);
  pos = sr->tag_table_pos + sr->tag_record_size * sr->tag_filled_records;
  ws = kernel_write(sr->storage_file, tag, sr->tag_record_size, &pos);
  if (ws != sr->tag_record_size) {
    res = -EFAULT;
    goto err;
  }

  res = IncTagFilledAmount(sr);
  // --------------
err:
  kfree(tag);
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


struct qstr tagfs_get_tag_name_by_index(Storage stor, size_t index) {
  struct StorageRaw* sr;
  struct qstr name = get_null_qstr();

  if (!stor) { return get_null_qstr(); }
  sr = (struct StorageRaw*)(stor);
  ReadTagName(sr, index, &name);
  return name;
}

size_t tagfs_get_tagino_by_name(Storage stor, const struct qstr name) {
  struct StorageRaw* sr;
  size_t i;
  struct qstr tn = get_null_qstr();

  if (!stor) { return kNotFoundIno; }
  sr = (struct StorageRaw*)(stor);
  for (i = 0; i < sr->tag_filled_records; ++i) {
    int res = ReadTagName(sr, i, &tn);
    int cr;

    if (res) {
      // Невосстановимая ошибка
      return kNotFoundIno;
    }

    cr = compare_qstr(tn, name);
    free_qstr(&tn);
    if (cr == 0) {
      return i;
    }
  }
  return kNotFoundIno;
}


// TODO PERFORMANCE Сделать подсчёт общего количества тэгов и делать быструю проверку на выход за существующее количество. Это будет часто запрашиваться
struct qstr tagfs_get_nth_tag(Storage stor, size_t index, size_t* tagino) {
  size_t i;
  size_t cur_index = 0;
  struct StorageRaw* sr;
  struct qstr name = get_null_qstr();

  if (!stor) { goto err; }
  sr = (struct StorageRaw*)(stor);

  for (i = 0; i < sr->tag_filled_records; ++i) {
    if (!CheckTagIsActive(sr, i)) { continue; }
    // Has found real/active tag. Check nth index
    if (cur_index != index) {
      ++cur_index;
      continue;
    }

    if (ReadTagName(sr, i, &name)) {
      goto err;
    }
    if (tagino) { *tagino = i; }
    return name;
  }
err:
  if (tagino) { *tagino = kNotFoundIno; }
  free_qstr(&name);
  return get_null_qstr();
}


struct qstr tagfs_get_next_tag(Storage stor, size_t* tagino) {
  size_t start = *tagino;
  struct StorageRaw* sr;
  struct qstr name = get_null_qstr();

  // Возьмём для старта следующую позицию
  if (start == (size_t)(-1)) {
    start = 0;
  } else {
    ++start;
  }

  if (!stor) { goto err; }
  sr = (struct StorageRaw*)(stor);
  while (start < sr->tag_filled_records) {
    if (ReadTagName(sr, start, &name)) {
      goto err;
    }

    if (name.len) {
      *tagino = start;
      return name;
    }

    ++start;
  }
  // ---------------
err:
  *tagino = kNotFoundIno;
  return get_null_qstr();
}


struct qstr tagfs_get_fname_by_ino(Storage stor, size_t ino,
    struct TagMask* mask) {
  struct StorageRaw* sr;
  struct qstr res;

  if (!stor) { return kNullQstr; }
  sr = (struct StorageRaw*)(stor);
  if (GetFileInfo(sr, ino, mask, &res, NULL)) { return kNullQstr; }
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

struct qstr tagfs_get_nth_file(Storage stor, const struct TagMask on_mask,
    const struct TagMask off_mask, size_t index, size_t* found_ino) {
  size_t i;
  size_t cur_index;
  struct StorageRaw* sr;

  if (!stor) { goto err; }
  sr = (struct StorageRaw*)(stor);

  for (i = 0, cur_index = 0; i < sr->fileblock_amount; ++i) {
    struct qstr res = get_null_qstr();
    struct TagMask mask = tagmask_empty();

    if (!CheckFileIsActive(sr, i)) { continue; }
    if (GetFileInfo(sr, i, &mask, &res, NULL)) { continue; }
    if (!tagmask_check_filter(mask, on_mask, off_mask)) { goto free_next; }

    if (cur_index == index) {
      if (found_ino) { *found_ino = i; }
      tagmask_release(&mask);
      return res;
    }

free_next:
    tagmask_release(&mask);
    free_qstr(&res);
  }

err:
  if (found_ino) { *found_ino = kNotFoundIno; }
  return get_null_qstr();
}


struct qstr tagfs_get_next_file(Storage stor, const struct TagMask on_mask,
    const struct TagMask off_mask, size_t* ino) {
  // TODO NEED TO IMPROVE PERFORMANCE
  size_t i;
  struct StorageRaw* sr;

  if (!stor) { goto err; }
  sr = (struct StorageRaw*)(stor);

  for (i = *ino + 1; i < sr->fileblock_amount; ++i) {
    struct qstr res = get_null_qstr();
    struct TagMask mask = tagmask_empty();

    if (!CheckFileIsActive(sr, i)) { continue; }
    if (GetFileInfo(sr, i, &mask, &res, NULL)) { continue; }
    if (!tagmask_check_filter(mask, on_mask, off_mask)) { goto free_next; }

    *ino = i;
    tagmask_release(&mask);
    return res;
    // ----------------------
free_next:
    tagmask_release(&mask);
    free_qstr(&res);
  }

err:
  *ino = kNotFoundIno;
  return get_null_qstr();
}


size_t tagfs_get_fileino_by_name(Storage stor, const struct qstr name,
    struct TagMask* mask) {
  // TODO NEED TO IMPROVE PERFORMANCE
  struct StorageRaw* sr;
  size_t i;

  if (!stor) { return kNotFoundIno; }
  sr = (struct StorageRaw*)(stor);
  for (i = 0; i < sr->fileblock_amount; ++i) {
    struct qstr res;
    int cmp;
    if (GetFileInfo(sr, i, mask, &res, NULL)) {
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


int tagfs_add_new_tag(Storage stor, const struct qstr tag_name) {
  struct StorageRaw* sr;

  if (!stor) { return -EINVAL; }
  sr = (struct StorageRaw*)(stor);
  return AddTag(sr, tag_name.name, tag_name.len);
}


size_t tagfs_get_maximum_tags_amount(Storage stor) {
  struct StorageRaw* sr;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  return sr->tag_record_max_amount;
}


int tagfs_set_file_mask(Storage stor, size_t fileino,
    const struct TagMask mask) {
  struct StorageRaw* sr;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  if (sr->tag_mask_byte_size != mask.byte_len) {
    pr_warn("%s:%i: Wrong mask size: %u vs %u", __FILE__, __LINE__,
        (unsigned int)sr->tag_mask_byte_size, (unsigned int)mask.byte_len);
    return -EFAULT;
  }
  if (UpdateDataIntoBlockChain(sr, fileino, mask.data, mask.byte_len,
      sizeof(struct FileHeader), 0) != mask.byte_len) {
    return -EFAULT;
  }

  return 0;
}


size_t tagfs_get_active_tags_amount(Storage stor) {
  struct StorageRaw* sr;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  return (size_t)sr->active_tag_amount;
}

