#include <linux/kernel.h>

#include "tag_dir.h"
#include "tag_inode.h"


int tagfs_dir_iterate(struct file* f, struct dir_context* dc) {
  printk(KERN_INFO "TODO dir iterate function with file %px, context %px\n", f, dc);
  return 0; // TODO There is no records
}

int tagfs_dir_open(struct inode *inode, struct file *file) {
  printk(KERN_INFO "TODO dir open function with inode %px, file %px\n", inode, file);
  return 0;
}

int tagfs_dir_release(struct inode *inode, struct file *file) {
  printk(KERN_INFO "TODO dir release function with inode %px, file %px\n", inode, file);
  return 0;
}

ssize_t tagfs_dir_read(struct file* filp, char __user* buf, size_t siz, loff_t* ppos) {
  printk(KERN_INFO "TODO dir read function\n");
  return -EISDIR;
}


struct dentry* tagfs_dir_lookup(struct inode* dir, struct dentry *dentry,
    unsigned int flags) {
  return NULL;
}

const struct inode_operations tagfs_dir_inode_ops = {
  .lookup = tagfs_dir_lookup
};


const struct file_operations tagfs_dir_file_ops = {
  .open = tagfs_dir_open,
  .release = tagfs_dir_release,
  .read = tagfs_dir_read,
  .iterate = tagfs_dir_iterate
};


int tagfs_init_dir(void) {
  return 0;
}


struct inode* tagfs_fills_dentry_by_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dir_index, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops) {
  struct inode* inode;

  inode = tagfs_create_inode(sb, S_IFDIR | 0755, dir_index);
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
  pr_info("  d_flags: %x (0x04 revalidate, 0x08 delete, 0x10 prune, 0x20 disconnected)\n", de->d_flags);
  tagfs_printk_inode(de->d_inode, 2);
}
