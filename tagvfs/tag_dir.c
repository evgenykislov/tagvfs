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

#include "tag_dir.h"

#include <linux/kernel.h>

#include "tag_inode.h"
#include "tag_storage.h"

/*! Счётчик ноды для директорий. Всегда растёт. Когда заполнится - директории
выводиться не будут, придётся перезапускать модуль файловой системы */
atomic_t dirino_counter = ATOMIC_INIT(kFSDirectoriesStartIno);


bool get_unique_dirino(size_t* dirino) {
  *dirino = atomic_inc_return(&dirino_counter);
  if (*dirino < kFSDirectoriesStartIno || *dirino > kFSDirectoriesFinishIno) {
    // Выход за допустимый диапазон индексов для директорий
    atomic_set(&dirino_counter, kFSDirectoriesFinishIno);
    return false;
  }
  return true;
}


/*! Создаём новую ноду для директории
\param sb указатель на суперблок файловой системы
\param dirino номер ноды для создаваемой директории. Может быть 0 - тогда ноде
будет назначен уникальный номер
\param inode_ops операции над нодой директории. Может быть NULL
\param file_ops файловые операции над директорией. Может быть NULL
\return указатель на новую ноду или отрицательный код ошибки */
struct inode* create_directory_inode(struct super_block* sb, size_t dirino,
    const struct inode_operations* inode_ops, const struct file_operations* file_ops) {
  struct inode* nod;

  if (dirino == 0) {
    if (!get_unique_dirino(&dirino)) {
      return ERR_PTR(-ENFILE);
    }
  }

  nod = tagfs_create_inode(sb, S_IFDIR | 0777, dirino);
  if (!nod) { return ERR_PTR(-ENOMEM); }
  // У созданной ноды уже установлены некоторые дефалтовые операции. Если нам
  // не нужно ставить свои, то ничего не меняем
  if (inode_ops) { nod->i_op = inode_ops; }
  if (file_ops) { nod->i_fop = file_ops; }
  return nod;
}


struct inode* fill_lookup_dentry_by_new_directory_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dirino, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops) {
  struct inode* nod;

  nod = create_directory_inode(sb, dirino, inode_ops, file_ops);
  if (IS_ERR(nod)) { return nod; }
  d_add(owner_de, nod);
  return nod;
}


struct inode*  fill_dentry_by_new_directory_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dirino,
    const struct inode_operations* inode_ops, const struct file_operations* file_ops) {
  struct inode* nod;

  nod = create_directory_inode(sb, dirino, inode_ops, file_ops);
  if (IS_ERR(nod)) { return nod; }
  d_instantiate(owner_de, nod);
  return nod;
}


void tagfs_printk_dentry(struct dentry* de) {
  pr_info("dentry: %px\n", de);
  if (!de) {
    pr_info("NULL\n");
    return;
  }
  pr_info("  d_name: '%30s'...\n", de->d_name.name);
  pr_info("  d_flags: %x (0x04 revalidate, 0x08 delete, 0x10 prune, 0x20 disconnected)\n",
      de->d_flags);
  tagfs_printk_inode(de->d_inode, 2);
}
