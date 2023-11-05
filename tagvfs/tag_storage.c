// This file is part of tagvfs
// Copyright (C) 2023 Evgeny Kislov
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "tag_storage.h"

#include <linux/fs.h>
#include <linux/slab.h>

#include "common.h"
#include "tag_storage_cache.h"

#define kTagHashBits 8
#define kFileHashBits 14

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
  __le16 reserved0; // Бывшее tag_filled_records; //!< Текущее количество заполненных записей тэгов. Учитывает удалённые тэги. Фактически позволяет определить позицию добавления нового тэга
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
  u16 tag_mask_byte_size; //!< Размер тэговой битовой маски (в байтах)

  u64 fileblock_table_pos;
  u16 fileblock_size;
  u64 fileblock_amount; // Переменная лочится fileblock_amount_lock

  size_t min_fileblock_for_seek_empty; // Номер блока, с которого можно начинать поиск свободного блока (для ускорения файловых операций)
  u16 last_added_tag_ino; // Номер тэга, который был добавлен последним (используется для поиска следующего места для добавления) TODO ATOMIC?

  struct qstr no_prefix;

  struct file* storage_file;
  rwlock_t fileblock_amount_lock; //!< Блокировка при расширении количества файловых блоков. Почему не на весь header - а остальное не меняется
  rwlock_t fileblock_lock; //!< Блокировка при изменении содержимого файловых блоков
  rwlock_t tag_lock; //!< Блокировка при изменении содержимого записей тэгов

  Cache file_cache; //!< Кэш файловых записей. Если запись неактивна, то имя пустое. Для активной записи пользовательские данные - FileData
  Cache tag_cache; //!< Кэш тегов. Если тэг неактивный, то имя пустое. Пользовательские данные всегда NULL.
};


struct FileData {
  struct TagMask tag_mask;
  struct qstr link_target;
};

void FileDataRemover(void* data) {
  struct FileData* fd = (struct FileData*)data;

  if (!fd) { return; }
  tagmask_release(&(fd->tag_mask));
  free_qstr(&(fd->link_target));
  kfree(fd);
}


const u16 kTagFlagFree = 0;
const u16 kTagFlagActive = 1;
const u16 kTagFlagBlocked = 2;
const u16 kTagFlagUndefined = (u16)(-1);


extern int TagFlagUpdate(struct StorageRaw* sr, size_t tagino, u16 expect_state,
    u16 new_state, const char* new_name, size_t new_name_len);

extern void ReadAllTagsToCache(struct StorageRaw* sr);
extern void ReadAllFilesToCache(struct StorageRaw* sr);


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
  sr->file_cache = NULL;
  sr->tag_cache = NULL;

  if ((res = tagfs_init_cache(&(sr->file_cache), kFileHashBits)) != 0) {
    goto err_ca;
  }

  if ((res = tagfs_init_cache(&(sr->tag_cache), kTagHashBits)) != 0) {
    goto err_ca;
  }

  f = filp_open(file_storage, O_RDWR, 0);
  if (IS_ERR(f)) {
    res = PTR_ERR(f);
    goto err_aa;
  }

  rpos = 0;
  rs = kernel_read(f, &(sr->header_mem), sizeof(struct FSHeader), &rpos);
  if (rs != sizeof(struct FSHeader)) {
    res = -EFAULT;
    goto err_ao;
  }

  sr->tag_table_pos = le64_to_cpu(sr->header_mem.tag_table_pos);
  sr->tag_record_size = le16_to_cpu(sr->header_mem.tag_record_size);
  sr->tag_record_max_amount = le16_to_cpu(sr->header_mem.tag_record_max_amount);
  sr->last_added_tag_ino = 0;
  sr->tag_mask_byte_size = tagmask_get_byte_len(sr->tag_record_max_amount);

  sr->fileblock_table_pos = le64_to_cpu(sr->header_mem.fileblock_table_pos);
  sr->fileblock_size = le16_to_cpu(sr->header_mem.fileblock_size);
  sr->fileblock_amount = le64_to_cpu(sr->header_mem.fileblock_amount);

  sr->min_fileblock_for_seek_empty = 0;

  if (sr->tag_record_max_amount == 0) {
    res = -EINVAL;
    goto err_ao;
  }


  sr->storage_file = f;
  rwlock_init(&sr->fileblock_amount_lock);
  rwlock_init(&sr->fileblock_lock);
  rwlock_init(&sr->tag_lock);

  sr->no_prefix = alloc_qstr_from_str("no-", 3);

  ReadAllTagsToCache(sr);
  ReadAllFilesToCache(sr);

  return 0;
  // --------------
err_ao:
  filp_close(f, NULL);
err_aa:
  tagfs_release_cache(sr->tag_cache);
  tagfs_release_cache(sr->file_cache);
err_ca:
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

  tagfs_release_cache(&sr->tag_cache);
  tagfs_release_cache(&sr->file_cache);

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
  void* tag_mem;
  u16 ti;


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
  h.tag_record_size = cpu_to_le16(kDefaultTagRecordSize); // TODO check value is dived by alignment
  h.tag_record_max_amount = cpu_to_le16(kDefaultTagRecordMaxAmount);
  h.reserved0 = cpu_to_le16(0);
  h.fileblock_size = cpu_to_le16(kDefaultFileBlockSize);
  h.fileblock_amount = cpu_to_le64(0);
  ws = kernel_write(f, &h, sizeof(h), &wpos);

  // Инициализируем место под тэги
  tag_mem = kzalloc(kDefaultTagRecordSize, GFP_KERNEL);
  if (!tag_mem) {
    res = -ENOMEM;
    goto ex;
  }

  wpos = tag_pos;
  for (ti = 0; ti < kDefaultTagRecordMaxAmount; ++ti) {
    if (kernel_write(f, tag_mem, kDefaultTagRecordSize, &wpos) !=
        kDefaultTagRecordSize) {
      res = -EFAULT;
      goto ex_mem;
    }
  }
ex_mem:
  kfree(tag_mem);
ex:
  res = filp_close(f, NULL);
  return res;
}


struct qstr kSpecNameOnlyFiles = QSTR_INIT("only-files", 10);
struct qstr kSpecNameFilesWOTags = QSTR_INIT("files-wo-tags", 13);
struct qstr kSpecNameTags = QSTR_INIT("tags", 4);
struct qstr kSpecNameOnlyTags = QSTR_INIT("only-tags", 9);
struct qstr kSpecNameControl = QSTR_INIT("control", 7);


struct qstr kNullQstr = QSTR_INIT(NULL, 0);
struct qstr kEmptyQStr = QSTR_INIT("", 0);

const size_t kNotFoundIno = (size_t)(-1);

/*! Возвращает текущее (возможно, устаревшее) значение количества файловых блоков */
u64 GetFileBlockAmount(struct StorageRaw* sr) {
  u64 v;
  read_lock(&sr->fileblock_amount_lock);
  v = sr->fileblock_amount;
  read_unlock(&sr->fileblock_amount_lock);
  return v;
}

/*! Увеличиваем общее количество файловых блоков и возвращаем новое значение
количества файловых блоков. Блокировка на файловую область не ставится.
Добавление делается в хвост файла, поэтому новый блок имеет номер (кол-во - 1)
\return обновлённое количество файловых блоков. Или отрицательный код ошибки */
u64 IncFileBlockAmountWOLock(struct StorageRaw* sr) {
  u64 v = -EFAULT;
  bool fba_failed = true;
  u64 fba = -EFAULT;
  loff_t pos = 0;
  struct FileBlockHeader bh;

  write_lock(&sr->fileblock_amount_lock);
  ++sr->fileblock_amount;
  sr->header_mem.fileblock_amount = cpu_to_le64(sr->fileblock_amount);
  pos = 0;
  if (kernel_write(sr->storage_file, &sr->header_mem, sizeof(struct FSHeader),
      &pos) == sizeof(struct FSHeader)) {
    fba = sr->fileblock_amount;
    fba_failed = false;
  }
  write_unlock(&sr->fileblock_amount_lock);
  if (fba_failed) { return kNotFoundIno; }

  // Инициализируем новый блок
  pos = sr->fileblock_table_pos + (fba - 1) * sr->fileblock_size;
  bh.prev_block_index = -2;
  bh.prev_block_index = -1;
  if (kernel_write(sr->storage_file, &bh, sizeof(bh), &pos) == sizeof(bh)) {
    v = fba;
  }

  return v;
}


/*! Вычитать имя тэга из файла хранилища. При чтении из файла используется блокировка
\param tag номер тэга
\param name указатель на переменную для возврата имени (входное содержимое переменное должна быть пустая строка). Не NULL
\return отрицательный код ошибки. 0 - ошибок нет и тэг активный. -ENOENT - такого тэга нет или он неактивен */
int ReadTagNameFromStorage(struct StorageRaw* sr, size_t tag, struct qstr* name) {
  void* tag_info;
  struct TagHeader* th;
  loff_t pos;
  int res = 0;
  size_t rs;

  if (unlikely(!name)) { return -EINVAL; }
  if (unlikely(name->name || name->len)) { return -EINVAL; }
  if (unlikely(!sr || !sr->storage_file)) { return -EINVAL; }

  if (tag >= sr->tag_record_max_amount) { return -EINVAL; }

  tag_info = kzalloc(sr->tag_record_size, GFP_KERNEL);
  if (!tag_info) { return -ENOMEM; }

  read_lock(&sr->tag_lock);

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

  if (th->tag_flags != kTagFlagActive) {
    tagfs_insert_item(sr->tag_cache, tag, get_null_qstr(), NULL, NULL); // Результат не проверяется, т.к. смысла нет
    res = -ENOENT;
    goto err;
  }

  *name = alloc_qstr_from_str(tag_info + sizeof(struct TagHeader),
      th->tag_name_size);

  // -----------------
err:
  read_unlock(&sr->tag_lock);
  kfree(tag_info);
  return res;
}


/*! Вычитать все тэги в кэш. Используется при открытии файловой системы */
void ReadAllTagsToCache(struct StorageRaw* sr) {
  size_t tag;

  WARN_ON(!sr || !sr->storage_file || !sr->tag_cache);
  for (tag = 0; tag < sr->tag_record_max_amount; ++tag) {
    struct qstr name = get_null_qstr();
    int res = ReadTagNameFromStorage(sr, tag, &name);

    if (res == -ENOENT) { continue; }
    if (res) {
      pr_warn("tagvfs: ERROR in reading tag %u\n", (unsigned int)tag);
      continue;
    }

    // Таг есть и активный
    if (tagfs_insert_item(sr->tag_cache, tag, name, NULL, NULL)) {
      pr_warn("tagvfs: ERROR in caching tag %u\n", (unsigned int)tag);
    }

    free_qstr(&name);
  }
}


/*! Вычитать имя тэга из кэша
\param tag номер тэга
\param name указатель на переменную для возврата имени (входное содержимое переменное должна быть пустая строка). Не NULL
\return отрицательный код ошибки. 0 - ошибок нет и тэг активный. -ENOENT - такого тэга нет или он неактивен */
int ReadTagName(struct StorageRaw* sr, size_t tag, struct qstr* name) {
  int res = 0;
  CacheIterator item;

  BUG_ON(!name);
  BUG_ON(!qstr_is_empty(*name));
  BUG_ON(!sr || !sr->tag_cache);

  item = tagfs_get_item_by_ino(sr->tag_cache, tag);
  if (!item) { return -ENOENT; }

  res = -ENOENT;
  if (item->Name.name && item->Name.len) {
    // Получили реальное имя
    if (name) { *name = alloc_qstr_from_qstr(item->Name); }
    res = 0; // Тэг активный
  }

  tagfs_release_item(item);
  return res;
}


/*! Вычитывает информацию о файле по номеру (ino). Для данных выделяется блок
 памяти. Здесь кэш не используется. Блокировка на файловую область не ставится.
\param stor описатель хранилища
\param ino номер файла
\param data указатель на возвращаемый буфер с данными. Должно быть *data == NULL
\param data_size указатель на размер данных
\return 0 - если всё хорошо. Иначе отрицательный код ошибки (-ENOENT - файл не существует) */
int AllocateReadFileDataWOLock(struct StorageRaw* sr, size_t ino, void** data,
                         size_t* data_size) {
  struct FileBlockHeader* block = NULL;
  loff_t pos;
  size_t rs;
  int res = 0;
  size_t cur_nod;
  size_t prev_nod;
  size_t block_counter;

  if (!sr || !sr->storage_file) { return -EINVAL; }
  if (!data || *data) { return -EINVAL; }
  if (!data_size) { return -EINVAL; }
  if (ino >= GetFileBlockAmount(sr)) { return -EINVAL; }
  if (sr->fileblock_size <= sizeof(struct FileBlockHeader)) { return -EINVAL; }

  *data_size = 0;

  block = kmalloc(sr->fileblock_size, GFP_KERNEL);
  if (!block) { return -ENOMEM; }

  prev_nod = ino;
  cur_nod = ino;
  block_counter = 0;
  while (block_counter < kMaxFileBlocks) {
    size_t new_size;
    size_t add_size;
    size_t next_block;

    pos = sr->fileblock_table_pos + cur_nod * sr->fileblock_size;
    rs = kernel_read(sr->storage_file, block, sr->fileblock_size, &pos);
    if (rs != sr->fileblock_size) {
      res = -EFAULT;
      goto err_allmem;
    }

    if (block_counter == 0 &&
        (block->prev_block_index == -1 || block->prev_block_index == -2)) {
      // Этот файл не существует или занят
      return -ENOENT;
      goto err_allmem;
    }
    if (block->prev_block_index != prev_nod) {
      res = -ESPIPE;
      goto err_allmem;
    }

    add_size = sr->fileblock_size - sizeof(struct FileBlockHeader);
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


/*! Освобождаем блок памяти с данными о файле, выделенный функцией
AllocateReadFileDataWOLock */
void FreeFileData(void** data, size_t* data_size) {
  if (data) {
    kfree(*data);
    *data = NULL;
  }
  if (data_size) {
    *data_size = 0;
  }
}


/*! Вычитываем информацию о файле из хранилища. Ставит блокировку на файловый блок
\param ino номер файла
\param tag_mask указатель на заполняемое поле маски (на вход должна быть пустая). Может быть NULL.
\param link_name указатель на заполняемое поле имени (на вход должно быть пустое). Может быть NULL.
\param link_target указатель на заполняемое поле целевой ссылки (на вход должна быть пустая). Может быть NULL.
\return отрицательный код ошибки (-ENOENT - файл не существует). Нет ошибок - 0 */
int ReadFileInfoFromStorage(struct StorageRaw* sr, size_t ino,
    struct TagMask* tag_mask, struct qstr* link_name, struct qstr* link_target) {
  void* data = NULL;
  size_t data_size = 0;
  int res;
  struct FileHeader* fh;
  size_t tagpos, namepos, targetpos;
  size_t taglen, namelen, targetlen;

  BUG_ON(!sr);
  BUG_ON(tag_mask && !tagmask_is_empty(*tag_mask));
  BUG_ON(link_name && !qstr_is_empty(*link_name));
  BUG_ON(link_target && !qstr_is_empty(*link_target));

  read_lock(&sr->fileblock_lock);
  res = AllocateReadFileDataWOLock(sr, ino, &data, &data_size);
  read_unlock(&sr->fileblock_lock);
  if (res) { return res; }

  fh = (struct FileHeader*)(data);
  tagpos = sizeof(struct FileHeader);
  if (tagpos > data_size) {
    res = -EFAULT;
    goto err;
  }

  taglen = le16_to_cpu(fh->tags_field_size);
  namepos = tagpos + taglen;
  namelen = le16_to_cpu(fh->link_name_size);
  targetpos = namepos + namelen;
  targetlen = le32_to_cpu(fh->link_target_size);
  if ((targetpos + targetlen) > data_size) {
    res = -EFAULT;
    goto err;
  }

  if (tag_mask) {
    *tag_mask = tagmask_init_zero(sr->tag_record_max_amount);
    tagmask_fill_from_buffer(*tag_mask, data + tagpos, taglen);
  }

  if (link_name) {
    *link_name = alloc_qstr_from_str(data + namepos, namelen);
  }

  if (link_target) {
    *link_target = alloc_qstr_from_str(data + targetpos, targetlen);
  }

  // -----------------
err:
  FreeFileData(&data, &data_size);
  return res;
}


/*! Вычитаем информацию о всех файлах в кэш */
void ReadAllFilesToCache(struct StorageRaw* sr) {
  size_t ino;
  size_t max_ino = GetFileBlockAmount(sr);

  for (ino = 0; ino < max_ino; ++ino) {
    struct qstr name = get_null_qstr();
    struct FileData* fd = kzalloc(sizeof(struct FileData), GFP_KERNEL);
    int res;

    if (!fd) {
      pr_warn("tagvfs: ERROR no mem for file caching\n");
      return;
    }

    fd->tag_mask = tagmask_empty();
    fd->link_target = get_null_qstr();

    res = ReadFileInfoFromStorage(sr, ino, &fd->tag_mask, &name, &fd->link_target);
    if (res) {
      if (res != -ENOENT) {
        pr_warn("tagvfs: ERROR Can't read file with ino %u\n", (unsigned int)ino);
      }

      kfree(fd);
      continue;
    }

    res = tagfs_insert_item(sr->file_cache, ino, name, fd, FileDataRemover);
    free_qstr(&name);
    if (res) {
      pr_warn("tagvfs: ERROR can't caching file info for ino %u\n", (unsigned int)ino);
    }
  }
}


/*! Вычитываем информацию о файле из кэша по номеру или по имени (если номер kNotFoundIno)
\param ino номер файла (может быть равен kNotFoundIno - тогда поиск по имени не производится)
\param name название файла (может быть пустым, если ino содержит валидное значение)
\param found_ino возвращает номер найденного файла. Может быть NULL.
\param found_name указатель на заполняемое поле имени. Может быть NULL.
\param tag_mask указатель на заполняемое поле маски. Может быть NULL.
\param link_target указатель на заполняемое поле целевой ссылки. Может быть NULL.
\return отрицательный код ошибки. Нет ошибок - 0 */
int GetFileInfo(struct StorageRaw* sr, size_t ino, const struct qstr name,
    size_t* found_ino, struct qstr* found_name, struct TagMask* tag_mask,
    struct qstr* link_target) {
  int res;
  CacheIterator item = NULL;

  WARN_ON(!sr);

  if (ino != kNotFoundIno) {
    item = tagfs_get_item_by_ino(sr->file_cache, ino);
  } else {
    item = tagfs_get_item_by_name(sr->file_cache, name);
  }
  if (!item) { return -ENOENT; }

  res = -ENOENT;
  if (item->Name.name && item->Name.len && item->user_data) {
    // Получили не пустой элемент
    struct FileData* fd = item->user_data;

    if (found_ino) {
      *found_ino = item->Ino;
    }
    if (found_name) {
      *found_name = alloc_qstr_from_qstr(item->Name);
    }
    if (tag_mask) {
      *tag_mask = tagmask_init_by_mask(fd->tag_mask);
    }
    if (link_target) {
      *link_target = alloc_qstr_from_qstr(fd->link_target);
    }

    res = 0;
  }

  tagfs_release_item(item);
  return res;
}


/*! Резервирует файловый блок: объявляет его занятым, но не в составе файла. Блокировка не ставится
\return номер зарезервированного блока или -1 (kNotFoundIno) в случае ошибки */
size_t ReserveFileBlockWOLock(struct StorageRaw* sr) {
  size_t ino;
  loff_t pos;
  struct FileBlockHeader bh;
  u64 fba = GetFileBlockAmount(sr);

  // Поищем существующий незанятый файловый блок
  for (ino = sr->min_fileblock_for_seek_empty; ino < fba; ++ino) {
    CacheIterator item;
    bool item_active = false;
    size_t rs;

    item = tagfs_get_item_by_ino(sr->file_cache, ino);
    if (item) {
      if (item->Name.name && item->Name.len && item->user_data) {
        item_active = true;
      }
      tagfs_release_item(item);
    }
    if (item_active) { continue; }

    pos = sr->fileblock_table_pos + ino * sr->fileblock_size;
    rs = kernel_read(sr->storage_file, &bh, sizeof(bh), &pos);
    if (rs != sizeof(struct FileBlockHeader)) {
      break;
    }

    if (bh.prev_block_index == -1) {
      sr->min_fileblock_for_seek_empty = ino;
      bh.prev_block_index = -2;
      if (kernel_write(sr->storage_file, &bh, sizeof(bh), &pos) == sizeof(bh)) {
        return ino;
      }
      return -EFAULT;
    }
  }

  // Попробуем добавить новый блок
  fba = IncFileBlockAmountWOLock(sr);
  if (IS_ERR_VALUE(fba) || !fba) {
    WARN_ON(fba == 0);
    return kNotFoundIno;
  }

  ino = fba - 1;
  return ino;
}


/*! Записывает часть информации о файле в файловый блок. Блокировка не ставится.
Если записались все данные (возвращаемое значение равно len), то блок закрывается
(поле next_block_index выставляется равным самому блоку). Если записались не все
данные, то индекс следующего блока выставляется в -1 и его нужно явно обновлять.
\param fb_index индекс файлового блока, куда записываются данные
\param prev_fb индекс предыдущего файлового блока, используется для построения цепочки блоков
\param data данные для записи
\param len длина данных для записи. Может быть больше, чем вмещается в блок
\return количество записанных байтов или 0 в случае ошибки */
size_t WriteFileBlockWOLock(struct StorageRaw* sr, size_t fb_index, size_t prev_fb,
    void* data, size_t len) {
  // TODO IMPROVE PERFORMANCE
  size_t avail;
  size_t zero_tail;
  struct FileBlockHeader h;
  loff_t pos;
  size_t ws;

  if (sr->fileblock_size <= sizeof(struct FileBlockHeader)) {
    pr_warn("Tagvfs: WARNING: file block size is less than header\n");
    return 0;
  }

  avail = sr->fileblock_size - sizeof(struct FileBlockHeader);
  if (avail > len) {
    avail = len;
  }
  zero_tail = sr->fileblock_size - sizeof(struct FileBlockHeader) - avail;

  h.prev_block_index = prev_fb;
  h.next_block_index = avail == len ? fb_index : -1;
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

  return avail;
}


/*! Связать файловый блок со следующим. Блокировка на файловую область не ставится.
\param fb_index номер файлового блока, который будет обновлён
\param fb_next номер следующего файлового блока
\return отрицательный код ошибки (0 - без ошибок) */
int UpdateNextFileBlockWOLock(struct StorageRaw* sr, size_t fb_index, size_t fb_next) {
  struct FileBlockHeader h;
  loff_t block_pos;
  loff_t pos;
  size_t ws;

  block_pos = sr->fileblock_table_pos + sr->fileblock_size * fb_index;
  pos = block_pos;
  ws = kernel_read(sr->storage_file, &h, sizeof(h), &pos);  // TODO PERFORMANCE - обойтись без чтения, сразу записать несколько байт
  if (ws != sizeof(h)) { return -EFAULT; }
  h.next_block_index = fb_next;
  pos = block_pos;
  ws = kernel_write(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) { return -EFAULT; }
  return 0;
}


/*! Очищает файловый блок - маркирует как свободный. Также возвращает номер
блока, который должен продолжать цепочку файловых блоков
\param fb_index индекс удаляемого файлового блока
\param prev_fb_index индекс предыдущего файлового блока. Используется для контроля цепочек блоков.
\param next_fb индекс файлового блока, который следующий в цепочке. Параметр может быть NULL
\return отрицательный код ошибки (-EINVAL - несовпадение предыдущего блока). 0 - если ошибок нет */
int ClearFileBlockWOLock(struct StorageRaw* sr, size_t fb_index,
    size_t prev_fb_index, size_t* next_fb) {
  struct FileBlockHeader h;
  loff_t block_pos;
  loff_t pos;
  size_t ws;

  block_pos = sr->fileblock_table_pos + sr->fileblock_size * fb_index;
  pos = block_pos;
  ws = kernel_read(sr->storage_file, &h, sizeof(h), &pos);
  if (ws != sizeof(h)) { return -EFAULT; }
  if (h.prev_block_index != prev_fb_index) { return -EINVAL;  }

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


/*! Переписать (обновить) данные в файловом описателе. Другие данные остаются
на месте, ничего не сдвигается. Если место записи выходит за границу существующей
цепочки блоков, то запись прерывается. Функция может вызываться рекурсивно.
Блокировка не ставится.
\param blockino номер блока, в котором обновляются данные
\param data данные для записи
\param data_size размер данных для обновления
\param data_pos позиция в файловом описателе, где обновлять данные
\param nest_counter счётчик вложенности вызовов. В начальном вызове должен быть 0
\return размер обновлённых данных */
size_t UpdateDataIntoBlockChainWOLock(struct StorageRaw* sr, size_t blockino,
    void* data, size_t data_size, size_t data_pos, size_t nest_counter) {
  struct FileBlockHeader h;
  loff_t block_pos;
  loff_t pos;
  size_t ws;
  size_t max_data = sr->fileblock_size - sizeof(h);

  if (nest_counter > kMaxFileBlocks) { return 0; }

  block_pos = sr->fileblock_table_pos + sr->fileblock_size * blockino;
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

    return ws + UpdateDataIntoBlockChainWOLock(sr, h.next_block_index, data + ws,
        data_size - ws, 0, nest_counter + 1);
  }

  // Нет записи в текущем блоке. Идём в следующий
  if (h.next_block_index == blockino) { return 0; }
  return UpdateDataIntoBlockChainWOLock(sr, h.next_block_index, data, data_size,
      data_pos - max_data, nest_counter + 1);
}


/*! Сохранить информацию о новом файле в хранилище. Используется блокировка на
файловую область.
\param link_name, link_name_len - название файла и длина имени
\param target_link, target_link_len - целевая ссылка и длина текстовой строки
\return номер (ino) созданного файла. Или отрицательный код ошибки */
size_t AddFileToStorage(struct StorageRaw* sr, const char* link_name, size_t link_name_len,
    const char* target_link, size_t target_link_len) {
  size_t file_info_size;
  void* file_info;
  size_t res = kNotFoundIno;
  struct FileHeader* fh;
  size_t pos;
  size_t fb_cur, fb_prev;
  void* chunk;
  size_t chunk_tail;
  size_t ino = kNotFoundIno;

  write_lock(&sr->fileblock_lock);

  // Сформируем информацию о файле
  file_info_size = sizeof(struct FileHeader) + sr->tag_mask_byte_size + link_name_len + target_link_len;
  file_info = kzalloc(file_info_size, GFP_KERNEL);
  if (!file_info) { goto err_nomem; }
  fh = (struct FileHeader*)(file_info);
  fh->tags_field_size = cpu_to_le16(sr->tag_mask_byte_size);
  fh->link_name_size = cpu_to_le16(link_name_len);
  fh->link_target_size = cpu_to_le16(target_link_len);
  pos = sizeof(struct FileHeader) + sr->tag_mask_byte_size;
  memcpy(file_info + pos, link_name, link_name_len);
  pos += link_name_len;
  memcpy(file_info + pos, target_link, target_link_len);

  // Резервируем свободный блок. Первый зарезервирвоанный блок будет номер файла
  ino = ReserveFileBlockWOLock(sr);
  if (ino == kNotFoundIno) {
    res = -EFAULT;
    goto err;
  }

  pr_info("TODO create new file with ino %u\n", (unsigned int)ino);

  fb_cur = ino;
  fb_prev = fb_cur;
  chunk = file_info;
  chunk_tail = file_info_size;

  // Запишем файл в хранилище
  while (chunk_tail > 0) {
    size_t red;

    red = WriteFileBlockWOLock(sr, fb_cur, fb_prev, chunk, chunk_tail);
    if (red == 0 || red > chunk_tail) {
      // TODO ERORR
      goto err;
    }

    if (fb_prev != fb_cur) {
      // Обновим ссылку на следующий блок у предыдущего блока
      // Не делается на первом блоке (когда fb_prev == fb_cur)
      res = UpdateNextFileBlockWOLock(sr, fb_prev, fb_cur);
      if (res) { goto err; }
    }

    chunk += red;
    chunk_tail -= red;

    if (chunk_tail == 0) {  // Так, записаны все данные. Просто выходим из цикла
      break;
    }

    fb_prev = fb_cur;
    fb_cur = ReserveFileBlockWOLock(sr);
    if (fb_cur == kNotFoundIno) {
      res = -EFAULT;
      goto err;
    }
  }

  res = ino;
  // ---------------------
err:
  kfree(file_info);
err_nomem:
  write_unlock(&sr->fileblock_lock);
  return res;
}


/*! Удалить запись о файле из хранилища. Используется блокировка на
файловую область.
\param fileino номер файла
\return отрицательный код ошибки. 0 - нет ошибок */
int DelFileFromStorage(struct StorageRaw* sr, size_t fileino) {
  size_t fi = fileino;
  int res = 0;
  size_t prev = fi;
  size_t i;

  write_lock(&sr->fileblock_lock);
  for (i = 0; i < kMaxFileBlocks; ++i) {
    size_t fn;

    res = ClearFileBlockWOLock(sr, fi, prev, &fn);
    if (res) {
      if (res == -EINVAL && fi == fileino) {
        // Ошибка неправильного аргумента на самом первом блоке.
        // Это означает, что файла с таким номером не существует
        // Уточняем код ошибки
        res = -ENOENT;
      }
      goto exit;
    }
    if (fi == fn) {
      // Должно res == 0
      goto exit;
    }
    fi = fn;
  }
  res = -EFBIG;
exit:
  write_lock(&sr->fileblock_lock);
  return res;
}


/*! Добавляет новый тэг (очевидно взамен некоторого свободного).
Функция использует блокировки
\param name, name_len имя тэга и длина имени
\param tagino возвращает номер (ino) созданного тэга. Может быть NULL
\return отрицательный код ошибки. 0 - нет ошибок */
int AddTagToStorage(struct StorageRaw* sr, const char* name, size_t name_len,
    size_t* tagino) {
  size_t i;
  int res;

  WARN_ON(sr->tag_record_max_amount == 0);

  if (sr->last_added_tag_ino >= sr->tag_record_max_amount) {
    sr->last_added_tag_ino = 0;
  }

  if (tagino) { *tagino = kNotFoundIno; }
  // Проверим старшие свободные номера
  for (i = sr->last_added_tag_ino + 1; i < sr->tag_record_max_amount; ++i) {
    res = TagFlagUpdate(sr, i, kTagFlagFree, kTagFlagActive, name, name_len);
    if (res == 0) {
      if (tagino) { *tagino = i; }
      sr->last_added_tag_ino = i;
      goto exit;
    };
  }

  // Проверим младшие свободные номера, включая последний
  for (i = 0; i <= sr->last_added_tag_ino; ++i) {
    res = TagFlagUpdate(sr, i, kTagFlagFree, kTagFlagActive, name, name_len);
    if (res == 0) {
      if (tagino) { *tagino = i; }
      sr->last_added_tag_ino = i;
      goto exit;
    };
  }
  res = -ENOENT;

exit:
  return res;
}


/*! Обновление состояния тэга (свободен/заблокирован/активен) и имени если
текущее состояние соответствует ожидаемому (expected_state). Если состояние не
соответствует желаемому, то информация о тэге не изменяется.
Допускаются переходы: active -> blocked -> free -> active.
При изменении состояния обрабатывается как кэш, так и хранилище. При обработке
хранилища ставится блокировка на тэговую область.
\param tagino номер (ino) тэга
\param expect_state ожидаемое состояние тэга (см. kTagFlagFree и др.)
\param new_state новое состояние
\param new_name, new_name_len новое имя (name и len) тэга при переходе free -> active.
При других переходах значения не используются и могут быть NULL
\return 0 - если состояние успешно изменилось. -EINVAL - если состояние тэга
не соответствует ожидаемому. -EFAULT - если произошла ошибка чтения/записи:
недостаточно прав для операции или ошибочный формат хранилища */
int TagFlagUpdate(struct StorageRaw* sr, size_t tagino, u16 expect_state,
    u16 new_state, const char* new_name, size_t new_name_len) {
  struct TagHeader th;
  loff_t basepos, pos;
  int res = 0;

  if (tagino >= sr->tag_record_max_amount) { return -EINVAL; }
  if (new_state == kTagFlagActive && (!new_name || !new_name_len)) {
    return -EINVAL;
  }

  if (expect_state == kTagFlagFree && new_state == kTagFlagActive) {
    struct qstr name;
    name.name = new_name;
    name.len = new_name_len;
    res = tagfs_insert_item(sr->tag_cache, tagino, name, NULL, NULL);
    if (res == -EEXIST) { return -EINVAL; }
    if (res) { return -EFAULT; }
  } else if (expect_state == kTagFlagActive && new_state == kTagFlagBlocked) {
    res = tagfs_hold_ino(sr->tag_cache, tagino);
    if (res== -ENOENT) { return -EINVAL; }
    if (res) { return -EFAULT; }
  } else if (expect_state == kTagFlagBlocked && new_state == kTagFlagFree) {
    const CacheIterator item = tagfs_get_item_by_ino(sr->tag_cache, tagino);
    if (!item) { return -EINVAL; }
    if (!(item->Name.name == NULL && item->Name.len == 0)) {
      tagfs_release_item(item);
      return -EINVAL;
    }

    // Удалим тэг из кэша
    tagfs_delete_item(sr->tag_cache, item);
    tagfs_release_item(item);
  }

  // Запишем изменения в хранилище
  write_lock(&sr->tag_lock);
  pos = basepos = sr->tag_table_pos + sr->tag_record_size * tagino;
  if (kernel_read(sr->storage_file, &th, sizeof(th), &pos) != sizeof(th)) {
    res = -EFAULT;
    goto err;
  }

  th.tag_flags = new_state;
  if (new_state == kTagFlagActive) {
    // Так как будет меняться имя, то выставим новый размер
    th.tag_name_size = sr->tag_record_size - sizeof(struct TagHeader);
    if (th.tag_name_size > new_name_len) {
      th.tag_name_size = new_name_len;
    }
  }
  pos = basepos;
  if (kernel_write(sr->storage_file, &th, sizeof(th), &pos) != sizeof(th)) {
    res = -EFAULT;
    goto err;
  }
  if (new_state == kTagFlagActive) {
    // Теперь поменяем имя
    pos = basepos + sizeof(struct TagHeader);
    if (kernel_write(sr->storage_file, new_name, th.tag_name_size, &pos) != th.tag_name_size) {
      res = -EFAULT;
      goto err;
    }
  }

  // -----------------
err:
  write_unlock(&sr->tag_lock);
  return res;
}


/*! Удалим тэг из всех файлов. Для поиска файлов используется информация из кэша.
При очистке ставится блокировка на файловый блок
\param tagino номер тэга, который будет удаляться
\return отрицательный код ошибки . 0 если нет ошибок */
int RemoveTagFromAllFiles(struct StorageRaw* sr, size_t tagino) {
  size_t i;
  int res = 0;
  u64 fba = GetFileBlockAmount(sr);

  for (i = 0; i < fba; ++i) {
    CacheIterator item;
    struct TagMask mask = tagmask_empty();

    write_lock(&sr->fileblock_lock);
    item = tagfs_get_item_by_ino(sr->file_cache, i);
    if (item) {
      if (item->Name.name && item->Name.len) {
        struct FileData* fd = item->user_data;
        mask = tagmask_init_by_mask(fd->tag_mask);
      }

      tagfs_release_item(item);
    }

    if (tagmask_check_tag(mask, tagino)) {
      // Обновляем значения в маске
      tagmask_set_tag(mask, tagino, false);
      if (UpdateDataIntoBlockChainWOLock(sr, i, mask.data, mask.byte_len,
          sizeof(struct FileHeader), 0) != mask.byte_len) { // TODO REMOVE MAGIC NUMBER (sizeof(struct FileHeader)) - add constant
        res = -EFAULT;
      }
    }

    tagmask_release(&mask);
    write_unlock(&sr->fileblock_lock);
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

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  ReadTagName(sr, index, &name); // В случае ошибки возвращается пустая строка и этого достаточно
  return name;
}


size_t tagfs_get_tagino_by_name(Storage stor, const struct qstr name) {
  struct StorageRaw* sr;
  CacheIterator item;
  size_t ino = kNotFoundIno;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  item = tagfs_get_item_by_name(sr->tag_cache, name);
  if (item) {
    if (item->Name.name && item->Name.len) {
      ino = item->Ino;
    }

    tagfs_release_item(item);
  }
  return ino;
}


struct qstr tagfs_get_nth_tag(Storage stor, size_t index,
    const struct TagMask exclude_mask, size_t* tagino) {
  size_t i;
  size_t cur_index = 0;
  struct StorageRaw* sr;
  struct qstr name = get_null_qstr();

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  for (i = 0; i < sr->tag_record_max_amount; ++i) {
    if (tagmask_check_tag(exclude_mask, i)) { continue; }
    if (ReadTagName(sr, index, &name) != 0) { continue; }
    // Has found real/active tag. Check nth index
    if (cur_index != index) {
      free_qstr(&name);
      ++cur_index;
      continue;
    }

    if (tagino) { *tagino = i; }
    return name;
  }

  // Нет таких тэгов
  if (tagino) { *tagino = kNotFoundIno; }
  return get_null_qstr();
}


struct qstr tagfs_get_next_tag(Storage stor, const struct TagMask exclude_mask,
    size_t* tagino) {
  size_t start = *tagino;
  struct StorageRaw* sr;
  struct qstr name = get_null_qstr();

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);

  // Возьмём для старта следующую позицию
  if (start == (size_t)(-1)) {
    start = 0;
  } else {
    ++start;
  }

  for (;start < sr->tag_record_max_amount; ++start) {
    int res;
    if (tagmask_check_tag(exclude_mask, start)) { continue; }
    res = ReadTagName(sr, start, &name);
    if (res == -ENOENT) { continue; } // Такого тэга просто нет. Штатная ситуация
    if (res) { break; }

    BUG_ON(name.len == 0);
    *tagino = start;
    return name;
  }

  // Тэгов больше нет или есть ошибки
  *tagino = kNotFoundIno;
  return get_null_qstr();
}


struct qstr tagfs_get_fname_by_ino(Storage stor, size_t ino,
    struct TagMask* mask) {
  struct StorageRaw* sr;
  struct qstr name;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  if (GetFileInfo(sr, ino, get_null_qstr(), NULL, &name, NULL, NULL)) {
    return kNullQstr;
  }
  return name;
}


struct qstr tagfs_get_special_name(Storage stor, enum FSSpecialName name) {
  struct qstr fixn = kNullQstr;
  switch (name) {
    case kFSSpecialNameOnlyFiles:   fixn = kSpecNameOnlyFiles; break;
    case kFSSpecialNameFilesWOTags: fixn = kSpecNameFilesWOTags; break;
    case kFSSpecialNameTags:        fixn = kSpecNameTags; break;
    case kFSSpecialNameOnlyTags:    fixn = kSpecNameOnlyTags; break;
    case kFSSpecialNameControl:     fixn = kSpecNameControl; break;
    case kFSSpecialNameUndefined:   fixn = kNullQstr; break;
  }

  // Создание копии строки сделано для единообразия API. В целом это,
  // конечно, просадка производительности
  return alloc_qstr_from_qstr(fixn);
}

enum FSSpecialName tagfs_get_special_type(Storage stor, struct qstr name) {
  if (compare_qstr(name, kSpecNameOnlyFiles) == 0) { return kFSSpecialNameOnlyFiles; }
  if (compare_qstr(name, kSpecNameFilesWOTags) == 0) { return kFSSpecialNameFilesWOTags; }
  if (compare_qstr(name, kSpecNameTags) == 0) { return kFSSpecialNameTags; }
  if (compare_qstr(name, kSpecNameOnlyTags) == 0) { return kFSSpecialNameOnlyTags; }
  if (compare_qstr(name, kSpecNameControl) == 0) { return kFSSpecialNameControl; }
  return kFSSpecialNameUndefined;
}


struct qstr tagfs_get_nth_file(Storage stor, const struct TagMask on_mask,
    const struct TagMask off_mask, size_t index, size_t* found_ino) {
  size_t i;
  size_t cur_index;
  struct StorageRaw* sr;
  u64 fba;

  if (index != 0) {
    pr_info("TagVfs Low Performance: request n-th file with non-zero (%u) index\n",
        (unsigned int)index); // TODO LOW PERFORMANCE
  }

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  fba = GetFileBlockAmount(sr);

  for (i = 0, cur_index = 0; i < fba; ++i) {
    struct qstr name = get_null_qstr();
    struct TagMask mask = tagmask_empty();

    if (GetFileInfo(sr, i, get_null_qstr(), NULL, &name, &mask, NULL)) { continue; }
    if (!tagmask_check_filter(mask, on_mask, off_mask)) { goto free_next; }

    if (cur_index == index) {
      if (found_ino) { *found_ino = i; }
      tagmask_release(&mask);
      return name;
    }

    ++cur_index;

free_next:
    tagmask_release(&mask);
    free_qstr(&name);
  }

  // Ничего не нашли, или файлов маловато
  if (found_ino) { *found_ino = kNotFoundIno; }
  return get_null_qstr();
}


struct qstr tagfs_get_next_file(Storage stor, const struct TagMask on_mask,
    const struct TagMask off_mask, size_t* ino) {
  // TODO NEED TO IMPROVE PERFORMANCE
  size_t i;
  struct StorageRaw* sr;
  u64 fba;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  fba = GetFileBlockAmount(sr);

  for (i = *ino + 1; i < fba; ++i) {
    struct qstr name = get_null_qstr();
    struct TagMask mask = tagmask_empty();

    if (GetFileInfo(sr, i, get_null_qstr(), NULL, &name, &mask, NULL)) { continue; }
    if (!tagmask_check_filter(mask, on_mask, off_mask)) { goto free_next; }

    *ino = i;
    tagmask_release(&mask);
    return name;
    // ----------------------
free_next:
    tagmask_release(&mask);
    free_qstr(&name);
  }

  // Нет следующего файла
  *ino = kNotFoundIno;
  return get_null_qstr();
}


size_t tagfs_get_fileino_by_name(Storage stor, const struct qstr name,
    struct TagMask* mask) {
  // TODO NEED TO IMPROVE PERFORMANCE
  struct StorageRaw* sr;
  size_t ino;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  if (GetFileInfo(sr, kNotFoundIno, name, &ino, NULL, mask, NULL)) {
    return kNotFoundIno;
  }
  return ino;
}


struct qstr tagfs_get_file_link(Storage stor, size_t ino) {
  struct StorageRaw* sr;
  struct qstr res;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  if (GetFileInfo(sr, ino, get_null_qstr(), NULL, NULL, NULL, &res) == 0) {
    return res;
  }

  return kNullQstr;
}


size_t tagfs_add_new_file(Storage stor, const char* target_name,
    const struct qstr link_name) {
  struct StorageRaw* sr;
  size_t ino;
  struct FileData* fd;
  size_t target_len = strlen(target_name);
  int res;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  ino = AddFileToStorage(sr, link_name.name, link_name.len, target_name,
      target_len);
  if (IS_ERR_VALUE(ino)) { return kNotFoundIno; }

  fd = kzalloc(sizeof(struct FileData), GFP_KERNEL);
  if (!fd) {
    pr_warn("tagvfs: ERROR no mem for file caching\n");
    return kNotFoundIno;
  }

  fd->tag_mask = tagmask_empty();
  fd->link_target = alloc_qstr_from_str(target_name, target_len);
  res = tagfs_insert_item(sr->file_cache, ino, link_name, fd, FileDataRemover);
  if (res) {
    pr_warn("tagvfs: ERROR %d for caching file information\n", res);
    return kNotFoundIno;
  }

  return ino;
}


int tagfs_del_file(Storage stor, const struct qstr file) {
  struct StorageRaw* sr;
  CacheIterator item;
  int res;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  item = tagfs_get_item_by_name(sr->file_cache, file);
  if (!item) { return -ENOENT; }
  tagfs_delete_item(sr->file_cache, item);

  res =  DelFileFromStorage(sr, item->Ino);
  tagfs_release_item(item);

  return res;
}


int tagfs_add_new_tag(Storage stor, const struct qstr tag_name, size_t* tagino) {
  struct StorageRaw* sr;
  int res;
  size_t tag;

  if (!stor) { return -EINVAL; }
  sr = (struct StorageRaw*)(stor);
  res =  AddTagToStorage(sr, tag_name.name, tag_name.len, &tag);
  if (res) { return res; }

  res = tagfs_insert_item(sr->tag_cache, tag, tag_name, NULL, NULL);
  if (res) {
    pr_warn("tagvfs: ERROR in caching tag %u\n", (unsigned int)tag);
    return res;
  }

  if (tagino) {
    *tagino = tag;
  }
  return 0;
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
  CacheIterator item;
  struct FileData* fd = NULL;
  struct FileData* prev_fd = NULL;
  int res;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);

  if (sr->tag_mask_byte_size != mask.byte_len) {
    pr_warn("%s:%i: Wrong mask size: %u vs %u", __FILE__, __LINE__,
        (unsigned int)sr->tag_mask_byte_size, (unsigned int)mask.byte_len);
    return -EFAULT;
  }

  item = tagfs_get_item_by_ino(sr->file_cache, fileino);
  if (!item) { return -ENOENT; }

  // TODO CHECK ITEM ISNT HOLD

  // Подготовим новый элемент на замену старому
  fd = kzalloc(sizeof(struct FileData), GFP_KERNEL);
  if (!fd) {
    pr_warn("tagvfs: ERROR no mem for file caching\n");
    goto item_err;
  }

  prev_fd = item->user_data;
  fd->tag_mask = tagmask_init_by_mask(mask);
  fd->link_target = alloc_qstr_from_qstr(prev_fd->link_target);
  if (!tagmask_is_empty(mask) && tagmask_is_empty(fd->tag_mask)) {
    goto item_err;
  }
  if (prev_fd->link_target.name && !fd->link_target.name) {
    goto item_err;
  }

  tagfs_delete_item(sr->file_cache, item);
  res = tagfs_insert_item(sr->file_cache, item->Ino, item->Name, fd, FileDataRemover);
  tagfs_release_item(item);

  if (!res) {
    size_t updated;
    write_lock(&sr->fileblock_lock);
    updated = UpdateDataIntoBlockChainWOLock(sr, fileino, mask.data, mask.byte_len,
        sizeof(struct FileHeader), 0);
    write_unlock(&sr->fileblock_lock);
    if (updated != mask.byte_len) {
      return -EFAULT;
    }
  }

  return 0;
  // --------------
item_err:
  FileDataRemover(fd);
  tagfs_release_item(item);
  return -ENOMEM;
}


const struct qstr tagfs_get_no_prefix(Storage stor) {
  struct StorageRaw* sr;

  BUG_ON(!stor);
  sr = (struct StorageRaw*)(stor);
  return sr->no_prefix;
}


int tagfs_del_tag(Storage stor, const struct qstr tag) {
  struct StorageRaw* sr;
  size_t tino;
  int tres;
  int fres;

  WARN_ON(!stor);
  if (!stor) { return -EINVAL; }
  sr = (struct StorageRaw*)(stor);
  tino = tagfs_get_tagino_by_name(stor, tag);
  if (tino == kNotFoundIno) { return -ENOENT; }

  tres = TagFlagUpdate(sr, tino, kTagFlagActive, kTagFlagBlocked, NULL, 0);
  if (tres) { return tres; }

  // Удалим упоминание о тэге во всех файлах
  fres = RemoveTagFromAllFiles(sr, tino);

  tres = TagFlagUpdate(sr, tino, kTagFlagBlocked, kTagFlagFree, NULL, 0);
  if (tres) { return tres; }

  return fres;
}
