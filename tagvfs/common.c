#include "common.h"

#include <linux/fs.h>


void* super_block_storage(const struct super_block* sb) {
  return sb->s_fs_info;
}

void* inode_storage(const struct inode* nod) {
  return nod->i_sb->s_fs_info;
}
