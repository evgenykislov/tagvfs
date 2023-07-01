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

#include "tag_onlytags_dir.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "common.h"
#include "tag_dir.h"
#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"

struct dir_data {
  size_t last_tag_ino; //!< номер тэга, последнего выданного в итерации
  loff_t last_tag_pos; //!< позиция тэга, последнего выданного в итерации. Или -1, если ничего не выдавалось
  loff_t after_tag_pos; //!< Позиция после всех тэгов. Или -1, если эта позиция ещё не определена
};


int emit_onlytags_dir_byname(struct dir_context* dc, const struct qstr name) {
  size_t dirino;

  if (!get_unique_dirino(&dirino)) { return -ENFILE; }
  if (!dir_emit(dc, name.name, name.len, dirino, DT_DIR)) { return -ENOMEM; }
  return 0;
}

int emit_onlytags_dir_byino(struct dir_context* dc, Storage stor, size_t ino) {
  int res;
  struct qstr name = tagfs_get_tag_name_by_index(stor, ino);

  if (!name.name) {
    return -ENOMEM;
  }

  res = emit_onlytags_dir_byname(dc, name);
  free_qstr(&name);
  return res;
}


int tagfs_onlytags_dir_iterate(struct file* f, struct dir_context* dc) {
  struct dir_data* dd = (struct dir_data*)(f->private_data);
  Storage stor = inode_storage(file_inode(f));
  struct TagMask zero = tagmask_init_zero(tagfs_get_maximum_tags_amount(stor));
  int res = 0;
  size_t ino;
  loff_t pos;
  WARN_ON(!dd);

  if (dd->after_tag_pos != -1 && dc->pos >= dd->after_tag_pos) { return 0; } // Все элементы выданы

  if (dc->pos < kPosAfterDots) {
    if (!dir_emit_dots(f, dc)) { return -ENOMEM; } // Добавим пару точек обязательных директорий
  }
  WARN_ON(dc->pos < kPosAfterDots);

  if (dc->pos < dd->last_tag_pos) {
    // Сбросим последнюю сохранённую позицию, т.к. требуется более ранний элемент
    // Например, кто-то сделал rewinddir или что-то подобное
    dd->last_tag_pos = -1;
  }

  // Найдём первый тэг (если не найден) и отдадим пользователю (если нужен, т.е. позиция == 2)
  if (dd->last_tag_pos == -1) {
    size_t ino;
    struct qstr name = tagfs_get_nth_tag(stor, 0, zero, &ino);
    if (ino == kNotFoundIno) {
      // Тэгов вообще нет
      dd->after_tag_pos = kPosAfterDots;
      return 0;
    }
    dd->last_tag_pos = kPosAfterDots;
    dd->last_tag_ino = ino;

    if (dc->pos == kPosAfterDots) {
      res = emit_onlytags_dir_byname(dc, name);
      if (res) {
        free_qstr(&name);
        return res;
      }
      ++dc->pos;
    }

    free_qstr(&name);
  }

  // Выдадим последний выданный элемент
  WARN_ON(dd->last_tag_pos == -1);
  WARN_ON(dc->pos < dd->last_tag_pos);
  if (dc->pos == dd->last_tag_pos) {
    res = emit_onlytags_dir_byino(dc, stor, dd->last_tag_ino);
    if (res) { return res; }
    ++dc->pos;
  }

  // Найдём остальные элементы и выдадим пользователю
  WARN_ON(dc->pos <= dd->last_tag_pos);
  ino = dd->last_tag_ino;
  pos = dd->last_tag_pos;
  while (1) {
    struct qstr name = tagfs_get_next_tag(stor, zero, &ino);
    ++pos;
    if (ino == kNotFoundIno) { break; }

    dd->last_tag_pos = pos;
    dd->last_tag_ino = ino;

    WARN_ON(pos > dc->pos);
    if (pos == dc->pos) {
      res = emit_onlytags_dir_byname(dc, name);
      if (res) {
        free_qstr(&name);
        return res;
      }
      ++dc->pos;
    }
    free_qstr(&name);
  }

  dd->after_tag_pos = pos;
  return 0;
}

int tagfs_onlytags_dir_open(struct inode *inode, struct file *file) {
  struct dir_data* dd;

  file->private_data = kzalloc(sizeof(struct dir_data), GFP_KERNEL);
  if (!file->private_data) { return -ENOMEM; }

  dd = (struct dir_data*)(file->private_data);
  dd->last_tag_ino = 0;
  dd->last_tag_pos = -1;
  dd->after_tag_pos = -1;
  return 0;
}

int tagfs_onlytags_dir_release(struct inode *inode, struct file *file) {
  kfree(file->private_data);
  file->private_data = NULL;
  return 0;
}

struct dentry* tagfs_onlytags_dir_lookup(struct inode* dir, struct dentry *de,
    unsigned int flags) {
  size_t ino;
  struct inode* inode;
  struct super_block* sb;
  Storage stor;

  sb = dir->i_sb;
  stor = super_block_storage(sb);
  ino = tagfs_get_tagino_by_name(stor, de->d_name);
  if (ino == kNotFoundIno) {
    d_add(de, NULL);
    return NULL;
  }

  inode = fill_lookup_dentry_by_new_directory_inode(sb, de, 0, NULL, NULL);
  if (!inode) { return ERR_PTR(-ENOMEM); }
  return NULL;
}


int tagfs_onlytags_dir_unlink(struct inode* dirnod, struct dentry* de) {
  return tagfs_del_tag(inode_storage(dirnod), de->d_name);
}


int tagfs_onlytags_dir_mkdir(struct inode* dir,struct dentry* de, umode_t mode) {
  return tagfs_add_new_tag(inode_storage(dir), de->d_name, NULL); // TODO CHECK EXISTANCE
}


int tagfs_onlytags_dir_rmdir(struct inode* dir,struct dentry* de) {
  return tagfs_del_tag(inode_storage(dir), de->d_name); // TODO CHECK EXISTANCE
}


const struct inode_operations tagfs_onlytags_dir_inode_ops = {
  .lookup = tagfs_onlytags_dir_lookup,
  .unlink = tagfs_onlytags_dir_unlink,
  .mkdir = tagfs_onlytags_dir_mkdir,
  .rmdir = tagfs_onlytags_dir_rmdir
};


const struct file_operations tagfs_onlytags_dir_file_ops = {
  .open = tagfs_onlytags_dir_open,
  .release = tagfs_onlytags_dir_release,
  .llseek = tagfs_common_dir_llseek,
  .read = generic_read_dir,
  .iterate_shared = tagfs_onlytags_dir_iterate
};
