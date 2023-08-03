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

#include <linux/kernel.h>
#include <linux/module.h>

#include "tag_fs.h"

#define TAGVFS_AUTHOR "Evgeny Kislov <dev@evgenykislov.com>"
#define TAGVFS_DESCRIPTION "VFS module to organize files by tags"

static int tagvfs_init(void) {
  int res = init_fs();
  if (res != 0) { return res; }

  return 0;
}

static void tagvfs_exit(void) {
  int res = exit_fs();
  if (res != 0) {
    printk(KERN_WARNING "Error in exit of filesystem: %d\n", res);
  }
}


module_init(tagvfs_init);
module_exit(tagvfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(TAGVFS_AUTHOR);
MODULE_DESCRIPTION(TAGVFS_DESCRIPTION);
