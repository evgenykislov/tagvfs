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

#include "tag_tag_dir.h"

#include <linux/slab.h>

#include "common.h"
#include "inode_info.h"
#include "tag_dir.h"
#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"

extern const struct dentry_operations tagfs_tag_dir_negative_dentry_ops;


struct FileInfo {
  // Маркеры для итерации по директории

  // Некоторая позиция (значения pos, tag, file) вычитывания содержимого директории
  // ..._pos - позиция в каталоге, с точки зрения linux (или -1, если позиции нет)
  // ..._tag - ino тэга (или -1 если позиция соответствует не тэгу)
  // ..._file - ino файла (или -1 если позиция соответствует не файлу)
  loff_t last_iterate_pos; //!< Позиция последней записи (возможно, неуспешной)
  size_t last_iterate_tag; //!< Номер тэга, соответствующий last_iterate_pos. Или kNotFoundIno - если позиция не тэг
  size_t last_iterate_file; //!< Номер файла, соответствующего last_iterate_pos. Или kNotFoundIno - если позиция не файл
  loff_t aftertag_pos; //!< Позиция в каталоге после последнего тэга (начинаются файлы)
};

void pr_info_FileInfo(const struct FileInfo* fi) {
  pr_info("FileInfo struct:\n");
  pr_info("last_iterate_pos %d, tag %u, file %u, aftertag %d\n",
      (int)fi->last_iterate_pos, (unsigned int)fi->last_iterate_tag,
      (unsigned int)fi->last_iterate_file, (int)fi->aftertag_pos);
}


struct dentry* tagfs_tag_dir_lookup(struct inode* dir, struct dentry *de,
    unsigned int flags) {
  struct inode* inode;
  struct super_block* sb;
  Storage stor;
  size_t tagino, fileino;
  struct InodeInfo* dir_info = get_inode_info(dir);
  struct TagMask mask = tagmask_empty();
  bool mask_suitable;
  struct qstr np;
  bool no_tag = false;

  sb = dir->i_sb;
  stor = super_block_storage(sb);

  np = qstr_trim_header_if_exist(de->d_name, tagfs_get_no_prefix(stor));
  if (np.name) {
    tagino = tagfs_get_tagino_by_name(stor, np);
    free_qstr(&np);
    no_tag = true;
  } else {
    tagino = tagfs_get_tagino_by_name(stor, de->d_name);
  }
  if (tagino != kNotFoundIno) {
    // Имя - это тэг
    struct InodeInfo* iinfo;
    size_t mask_len;

    inode = fill_lookup_dentry_by_new_directory_inode(sb, de, 0, &tagfs_tag_dir_inode_ops,
        &tagfs_tag_dir_file_ops);
    if (IS_ERR(inode)) {
      d_add(de, NULL);
      return NULL;
    }

    iinfo = get_inode_info(inode);
    iinfo->tag_ino = tagino;
    iinfo->on_tag = !no_tag;
    WARN_ON(!tagmask_is_empty(iinfo->on_mask));
    WARN_ON(!tagmask_is_empty(iinfo->off_mask));
    mask_len = tagfs_get_maximum_tags_amount(stor);
    iinfo->on_mask = tagmask_init_by_mask(dir_info->on_mask);
    iinfo->off_mask = tagmask_init_by_mask(dir_info->off_mask);
    if (no_tag) {
      tagmask_set_tag(iinfo->off_mask, tagino, true);
    } else {
      tagmask_set_tag(iinfo->on_mask, tagino, true);
    }

    return NULL;
  }

  // Имя - это файл
  fileino = tagfs_get_fileino_by_name(stor, de->d_name, &mask);
  mask_suitable = tagmask_check_filter(mask, dir_info->on_mask,
      dir_info->off_mask);
  tagmask_release(&mask);
  if (!mask_suitable || fileino == kNotFoundIno) {
    d_set_d_op(de, &tagfs_tag_dir_negative_dentry_ops);
    d_add(de, NULL);
    return NULL;
  }

  inode = tagfs_fills_dentry_by_linkfile_inode(sb, de, fileino + kFSRealFilesStartIno);
  if (!inode) { return ERR_PTR(-ENOMEM); }
  return NULL;
}


/*! Колбэк на создание символьной ссылки в каталоге. В колбэк передаётся негативный
dentry, который нужно дополнить новым inode.
\param dir директория, в которой создаётся новая ссылка
\param de заголовка (негативная) на элемент в директории
\param name путь к целевому файлу (куда указывает ссылка)
\return код ошибки (0 - ошибки нет) */
int tagfs_tag_dir_symlink(struct inode* dir, struct dentry* de, const char* name) {
  struct inode* newnode;
  size_t ino;
  Storage stor = inode_storage(dir);
  struct InodeInfo* dir_info = get_inode_info(dir);
  size_t exist_ino;
  struct TagMask mask;
  int res = 0;

  exist_ino = tagfs_get_fileino_by_name(stor, de->d_name, &mask);
  if (exist_ino != kNotFoundIno) {
    struct qstr target = get_null_qstr();
    struct qstr income = alloc_qstr_from_str(name, strlen(name)); // TODO PERFORMANCE. REMOVE allocation
    target = tagfs_get_file_link(stor, exist_ino);
    if (!income.name) {
      res = -ENOMEM;
      goto exist_err;
    }

    if (compare_qstr(target, income) != 0) {
      // Попытка создать символьную ссылку с существующим именем, указывающую на
      // другой файл. Говорим, что такой файл уже есть
      res = -EEXIST;
      goto exist_err;
    }

    // Это либо копирование нашей же ссылки из папки в папку, либо создание ссылки
    // на тот же самый файл. В любом случае операцию делаем и двигаем маску тэгов.
    tagmask_or_mask(mask, dir_info->on_mask);
    tagmask_exclude_mask(mask, dir_info->off_mask);
    res = tagfs_set_file_mask(stor, exist_ino, mask);
    tagmask_release(&mask);
    WARN_ON(de->d_inode);
    newnode = tagfs_create_inode(dir->i_sb, S_IFLNK | 0777, exist_ino + kFSRealFilesStartIno);
    if (!newnode) {
      res = -ENOMEM;
      goto exist_err;
    }
    d_instantiate(de, newnode);
    // -------
exist_err:
    free_qstr(&income);
    free_qstr(&target);
    tagmask_release(&mask);
    return res;
  }

  // Создаём новый файл
  ino = tagfs_add_new_file(stor, name, de->d_name);
  newnode = tagfs_create_inode(dir->i_sb, S_IFLNK | 0777, ino + kFSRealFilesStartIno);
  if (!newnode) {
    res = -ENOMEM;
    goto new_err;
  }
  tagfs_set_linkfile_operations_for_inode(newnode);
  d_instantiate(de, newnode);
  // ---------------
new_err:
  tagmask_release(&mask);
  return 0;
}

/*! Колбэк на операцию по удалению файла для ноды
\param dir нода директории, в которой что-то удаляется
\param de dentry на удаляемый элемент
\return 0 при успешном удалении или отрицательный код ошибки */
int tagfs_tag_dir_unlink(struct inode* dir, struct dentry* de) {
  struct InodeInfo* dir_info = get_inode_info(dir);
  Storage stor = inode_storage(dir);
  struct TagMask mask = tagmask_empty();
  struct qstr name = get_null_qstr();
  int res;
  size_t fileino;
  struct inode* fi = d_inode(de);
  bool mask_bit;

  if (!fi) { return -ENOENT; }

  if (fi->i_ino < kFSRealFilesStartIno || fi->i_ino > kFSRealFilesFinishIno) {
    // Удалять в дереве тэгов можно только файлы. Остально - запрещено.
    return -EPERM;
  }

  fileino = fi->i_ino - kFSRealFilesStartIno;
  name = tagfs_get_fname_by_ino(stor, fileino, &mask);
  if (!name.name) {
    // Файл не существует
    return -ENOENT;
  }
  free_qstr(&name);

  // Модифицируем маску файла: удаляем/добавляем соответствующий тэг
  mask_bit = tagmask_check_tag(mask, dir_info->tag_ino);
  if (dir_info->on_tag) {
    if (mask_bit) { tagmask_set_tag(mask, dir_info->tag_ino, false); }
    else { res = -EINVAL; }
  } else {
    if (!mask_bit) { tagmask_set_tag(mask, dir_info->tag_ino, true); }
    else { res = -EINVAL; }
  }
  res = tagfs_set_file_mask(stor, fileino, mask);
  tagmask_release(&mask);

  d_set_d_op(de, &tagfs_tag_dir_negative_dentry_ops);

  return res;
}


int tagfs_tag_dir_mkdir(struct inode* dir,struct dentry* de, umode_t mode) {
  size_t tagino;
  int res;
  struct inode* newnode;
  struct InodeInfo* dir_info = get_inode_info(dir);
  struct InodeInfo* new_info;
  size_t mask_len;
  struct super_block* sb = dir->i_sb;

  Storage stor = inode_storage(dir);
  res = tagfs_add_new_tag(stor, de->d_name, &tagino); // TODO CHECK EXISTANCE
  if (res) { return res; }

  newnode = fill_dentry_by_new_directory_inode(sb, de, 0, &tagfs_tag_dir_inode_ops,
      &tagfs_tag_dir_file_ops);
  if (IS_ERR(newnode)) { return PTR_ERR(newnode); }

  new_info = get_inode_info(newnode);
  new_info->tag_ino = tagino;
  new_info->on_tag = true;
  WARN_ON(!tagmask_is_empty(new_info->on_mask));
  WARN_ON(!tagmask_is_empty(new_info->off_mask));
  mask_len = tagfs_get_maximum_tags_amount(stor);
  new_info->on_mask = tagmask_init_by_tag(mask_len, tagino);
  new_info->off_mask = tagmask_init_zero(mask_len);
  tagmask_or_mask(new_info->on_mask, dir_info->on_mask);
  tagmask_or_mask(new_info->off_mask, dir_info->off_mask);
  return 0;
}


int tagfs_tag_dir_iterate_tag(struct dir_context* dc, Storage stor,
    struct FileInfo* fi, const struct InodeInfo* iinfo) {
  const size_t kAfterDotsPos = 2; //!< Позиция после стандартных записей с одной и двумя точками
  const size_t kRecordsPerTag = 2;
  size_t tagpos = kAfterDotsPos;
  size_t tagino;
  struct TagMask excl_mask;
  int res = 0;

  struct qstr noprefix = tagfs_get_no_prefix(stor);
  // Готовим маску с тэгами, которые уже были (excl_mask)
  excl_mask = tagmask_init_by_mask(iinfo->on_mask);
  tagmask_or_mask(excl_mask, iinfo->off_mask);


  BUG_ON(dc->pos < kAfterDotsPos);
  if (fi->last_iterate_pos == -1) {
    // Запрос информации на самый первый тэг
    struct qstr name = tagfs_get_nth_tag(stor, 0, excl_mask, &tagino);
    struct qstr noname = get_null_qstr();
    size_t dirino;

    if (!name.name) {
      fi->aftertag_pos = kAfterDotsPos;
      goto ex;
    }

    noname = qstr_add_header(name, noprefix); // TODO CHECK noname isn't empty

    fi->last_iterate_pos = kAfterDotsPos;
    fi->last_iterate_tag = tagino;
    // Первый тэг с "прямым" именем
    if (dc->pos == kAfterDotsPos) {
      if (!get_unique_dirino(&dirino)) { res = -ENFILE; goto ft_err; }
      if (!dir_emit(dc, name.name, name.len, dirino, DT_DIR)) { res = -ENOMEM; goto ft_err; }
      ++dc->pos;
    }

    // Второй тэг с негативным именем
    if (dc->pos == kAfterDotsPos + 1) {
      if (!get_unique_dirino(&dirino)) { res = -ENFILE; goto ft_err; }
      if (!dir_emit(dc, noname.name, noname.len, dirino, DT_DIR)) { res = -ENOMEM; goto ft_err; }
      ++dc->pos;
    }

ft_err:
    free_qstr(&name);
    free_qstr(&noname);
    if (res) { goto ex; }
  }

  BUG_ON(dc->pos < kAfterDotsPos + kRecordsPerTag);
  BUG_ON(fi->last_iterate_pos == -1);
  BUG_ON(fi->last_iterate_tag == kNotFoundIno);
  BUG_ON(dc->pos < (fi->last_iterate_pos + kRecordsPerTag));
  tagino = fi->last_iterate_tag;
  tagpos = fi->last_iterate_pos + kRecordsPerTag;
  while (true) {
    struct qstr noname = get_null_qstr();
    struct qstr name = tagfs_get_next_tag(stor, excl_mask, &tagino);
    size_t dirino;

    if (!name.name) {
      fi->aftertag_pos = tagpos;
      goto ex;
    }

    fi->last_iterate_pos = tagpos;
    fi->last_iterate_tag = tagino;

    if (tagpos == dc->pos) {
      if (!get_unique_dirino(&dirino)) { res = -ENFILE; goto st_err; }
      if (!dir_emit(dc, name.name, name.len, dirino, DT_DIR)) { res = -ENOMEM; goto st_err; }
      ++dc->pos;
    }
    ++tagpos;

    if (tagpos == dc->pos) {
      struct qstr noname = qstr_add_header(name, noprefix); // TODO CHECK noname isn't empty
      if (!get_unique_dirino(&dirino)) { res = -ENFILE; goto st_err; }
      if (!dir_emit(dc, noname.name, noname.len, dirino, DT_DIR)) { res = -ENOMEM; goto st_err; }
      ++dc->pos;
    }
    ++tagpos;

st_err:
    free_qstr(&name);
    free_qstr(&noname);
    if (res) { goto ex; }
  }

ex:
  tagmask_release(&excl_mask);
  return res;
}


int tagfs_tag_dir_iterate_file(struct dir_context* dc, Storage stor,
    struct FileInfo* fi, const struct InodeInfo* iinfo) {
  size_t file_start;
  size_t file_ino;
  size_t file_pos;

  // Перейдём к обработке файлов
  file_start = dc->pos - fi->aftertag_pos;
  if (fi->last_iterate_file == -1 || file_start == 0) {
    // Выдадим самый первый файл
    struct qstr name = tagfs_get_nth_file(stor, iinfo->on_mask, iinfo->off_mask,
        0, &file_ino);
    if (!name.name) {
      // Нету файлов. Вообще
      return 0;
    }

    if (dc->pos == fi->aftertag_pos) {
      if (!dir_emit(dc, name.name, name.len, file_ino + kFSRealFilesStartIno, DT_LNK)) {
        free_qstr(&name);
        return -ENOMEM;
      }
      dc->pos += 1;
    }

    fi->last_iterate_pos = fi->aftertag_pos;
    fi->last_iterate_file = file_ino;
    fi->last_iterate_tag = -1;
    free_qstr(&name);
  }

  // Выдадим остальные файлы
  BUG_ON(dc->pos <= fi->last_iterate_pos);
  BUG_ON(fi->last_iterate_file == -1);
  BUG_ON(fi->last_iterate_tag != -1);
  file_ino = fi->last_iterate_file;
  file_pos = fi->last_iterate_pos;
  while (true) {
    struct qstr name = tagfs_get_next_file(stor, iinfo->on_mask, iinfo->off_mask,
        &file_ino);
    if (!name.name) {
      return 0;
    }
    ++file_pos;

    if (dc->pos == file_pos) {
      if (!dir_emit(dc, name.name, name.len, file_ino + kFSRealFilesStartIno, DT_LNK)) {
        free_qstr(&name);
        return -ENOMEM;
      }
      dc->pos += 1;
    }

    fi->last_iterate_pos = file_pos;
    fi->last_iterate_file = file_ino;
    free_qstr(&name);
  }
}


int tagfs_tag_dir_iterate(struct file* f, struct dir_context* dc) {
  Storage stor;
  struct InodeInfo* iinfo;
  struct FileInfo* fi;
  int res = 0;


  stor = inode_storage(file_inode(f)); BUG_ON(!stor);
  iinfo = get_inode_info(file_inode(f)); BUG_ON(!iinfo);
  fi = f->private_data; BUG_ON(!fi);

  // Поиск до позиции последней выдачи. Начинаем всё сначала
  if (fi->last_iterate_pos >= dc->pos) {
    fi->last_iterate_pos = -1;
    fi->last_iterate_tag = -1;
    fi->last_iterate_file = -1;
    fi->aftertag_pos = -1;
  }

  // проверим, нужно ли обрабатывать тэги и директории-точки
  if (fi->aftertag_pos == -1 || fi->aftertag_pos > dc->pos) {
    if (!dir_emit_dots(f, dc)) { return -ENOMEM; }
    res = tagfs_tag_dir_iterate_tag(dc, stor, fi, iinfo);
    if (res) { return res; }
  }

  return tagfs_tag_dir_iterate_file(dc, stor, fi, iinfo);
}

int tagfs_tag_dir_open(struct inode* dir, struct file* f) {
  struct FileInfo* fi;

  fi = (struct FileInfo*)kzalloc(sizeof(struct FileInfo), GFP_KERNEL);
  if (fi == NULL) { return -ENOMEM; }
  f->private_data = fi;
  fi->last_iterate_pos = -1;
  fi->last_iterate_tag = kNotFoundIno;
  fi->last_iterate_file = kNotFoundIno;
  fi->aftertag_pos = -1;

  return 0;
}

int tagfs_tag_dir_release(struct inode* dir, struct file* f) {
  WARN_ON(!f->private_data);
  kfree(f->private_data);
  return 0;
}

loff_t tagfs_tag_dir_llseek(struct file* f, loff_t offset, int whence) {
  switch (whence) {
    case 0: // absolute offset
      f->f_pos = offset;
      break;
    case 1: // relative offset
      f->f_pos += offset;
      break;
    default:
      f->f_pos = -1;
  }

  if (f->f_pos >= 0) {
    return f->f_pos;
  }
  return -EINVAL;
}


int tagfs_tag_dir_revalidate(struct dentry* de, unsigned int flags) {
  return 0;
}

const struct inode_operations tagfs_tag_dir_inode_ops = {
  .lookup = tagfs_tag_dir_lookup,
  .symlink = tagfs_tag_dir_symlink,
  .unlink = tagfs_tag_dir_unlink,
  .mkdir = tagfs_tag_dir_mkdir
};

const struct file_operations tagfs_tag_dir_file_ops = {
  .open = tagfs_tag_dir_open,
  .release = tagfs_tag_dir_release,
  .llseek = tagfs_tag_dir_llseek,
  .read = generic_read_dir,
  .iterate_shared = tagfs_tag_dir_iterate
};

const struct dentry_operations tagfs_tag_dir_negative_dentry_ops = {
  .d_revalidate = tagfs_tag_dir_revalidate
};
