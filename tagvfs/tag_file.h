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

#ifndef TAG_FILE_H
#define TAG_FILE_H

#include <linux/fs.h>
#include <linux/kernel.h>


/*! Добавляет к негативному dentry новый inode для файла-линка. Операции на файлами
прописаны внутренние (внешние не предполагаются).
\param sb суперблок
\param owner_de структура dentry, к которой добавляется inode
\param file_index номер inode-а, который будет добавлен к dentry
\return либо указатель на созданный inode, либо NULL если есть ошибки */
struct inode* tagfs_fills_dentry_by_linkfile_inode(struct super_block* sb,
    struct dentry* owner_de, size_t file_index);


void tagfs_set_linkfile_operations_for_inode(struct inode* nod);

#endif // TAG_FILE_H
