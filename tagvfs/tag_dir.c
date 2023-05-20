#include <linux/kernel.h>

#include "tag_dir.h"
#include "tag_inode.h"


int tagfs_dir_iterate(struct file* f, struct dir_context* dc) {
  return 0; // TODO There is no records
}

int tagfs_dir_open(struct inode *inode, struct file *file) {
  return 0;
}

int tagfs_dir_release(struct inode *inode, struct file *file) {
  return 0;
}

struct dentry* tagfs_dir_lookup(struct inode* dir, struct dentry *dentry,
    unsigned int flags) {
  return NULL;
}

// TODO REMOVE???
const struct inode_operations tagfs_dir_inode_ops = {
  .lookup = tagfs_dir_lookup
};


const struct file_operations tagfs_dir_file_ops = {
  .open = tagfs_dir_open,
  .release = tagfs_dir_release,
  .iterate = tagfs_dir_iterate
};


struct inode* tagfs_fills_dentry_by_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dir_index, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops) {
  struct inode* inode;

  inode = tagfs_create_inode(sb, S_IFDIR | 0777, dir_index);
  if (!inode) { return NULL; }

  inode->i_op = inode_ops;
  inode->i_fop = file_ops;
  d_add(owner_de, inode);
  return inode;
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
