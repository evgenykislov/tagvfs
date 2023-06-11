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

#ifndef TAG_INODE_H
#define TAG_INODE_H

#include <linux/fs.h>
#include <linux/kernel.h>

#include "inode_info.h"

/*! Создаём ни к чему не привязанный inode
\param sb СуперБлок файловой системы
\param mode Режим файла/сущности. Это и тип, и биты доступа
\param index Индекс файла/сущности в кастомной файловой системе */
struct inode* tagfs_create_inode(struct super_block* sb, umode_t mode,
    size_t index);


/* ??? */
struct inode* tagfs_inode_alloc(struct super_block *sb);

/* ??? */
void tagfs_inode_free(struct inode* nod);


/* ??? */
struct InodeInfo* get_inode_info(struct inode* nod);


void tagfs_printk_inode(const struct inode* ind, unsigned int indent);

#endif // TAG_INODE_H
