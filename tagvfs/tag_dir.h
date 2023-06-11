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

#ifndef TAG_DIR_H
#define TAG_DIR_H

#include <linux/fs.h>
#include <linux/kernel.h>

extern const struct inode_operations tagfs_dir_inode_ops;
extern const struct file_operations tagfs_dir_file_ops;

/*! Выдаёт уникальный номер ноды для директории.
\param dirino указатель для получения уникального номера. Не может быть NULL.
Значение под указателем может быть изменено даже в случае ошибок
\return признак успешности выдачи номера */
bool get_unique_dirino(size_t* dirino);

/*! Добавляет к негативному dentry новый inode для колбэка lookup
\param sb суперблок
\param owner_de структура dentry, к которой добавляется inode
\param dir_index номер inode-а, который будет добавлен к dentry
\param inode_ops структура с inode операциями inode-а (может быть NULL)
\param file_ops структура с файловыми операциями inode-а (может быть NULL)
\return либо указатель на созданный inode, либо NULL если есть ошибки */
struct inode* fill_lookup_dentry_by_new_directory_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dirino, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops);


/*! Создаёт новый inode для директории и заполняет информацию о нём в owner_de
\param sb указатель на суперблок
\param owner_de структура dentry, к которой привязать новый созданый нод
\param dirino номер ноды для директории. Если значение 0 - то генерируется следующий уникальный
\param inode_ops операции ноды для директории. Может быть NULL
\param file_ops файловые операции для директории. Может быть NULL
\return указатель на новый нод или отрицательный код ошибки */
struct inode* fill_dentry_by_new_directory_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dirino,
    const struct inode_operations* inode_ops, const struct file_operations* file_ops);

int tagfs_dir_iterate(struct file* f, struct dir_context* dc);

void tagfs_printk_dentry(struct dentry* de);


#endif // TAG_DIR_H
