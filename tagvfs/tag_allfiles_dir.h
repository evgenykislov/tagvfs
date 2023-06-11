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

#ifndef TAG_ALLFILES_DIR_H
#define TAG_ALLFILES_DIR_H

#include <linux/kernel.h>

extern const struct inode_operations tagfs_allfiles_dir_inode_ops;
extern const struct file_operations tagfs_allfiles_dir_file_ops;

#endif // TAG_ALLFILES_DIR_H
