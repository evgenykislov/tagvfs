#include "tag_file.h"

#include "tag_inode.h"


int tagfs_file_iterate(struct file* f, struct dir_context* dc) {
  printk(KERN_INFO "TODO file iterate function with file %px, context %px\n", f, dc);
  return -ENOMEM; // TODO There is no records
}

int tagfs_file_open(struct inode *inode, struct file *file) {
  printk(KERN_INFO "TODO file open function with inode %px, file %px\n", inode, file);
  return -ENOMEM;
}

int tagfs_file_release(struct inode *inode, struct file *file) {
  printk(KERN_INFO "TODO file release function with inode %px, file %px\n", inode, file);
  return -ENOMEM;
}

ssize_t tagfs_file_read(struct file* filp, char __user* buf, size_t siz, loff_t* ppos) {
  printk(KERN_INFO "TODO file read function\n");
  return -EISDIR;
}


static struct file_operations tagfs_file_ops = {
  .open = tagfs_file_open,
  .release = tagfs_file_release,
  .read = tagfs_file_read,
  .iterate = tagfs_file_iterate
};

struct inode* tagfs_fills_file_dentry_by_inode(struct super_block* sb,
    struct dentry* owner_de, size_t file_index, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops) {
  struct inode* inode;

  inode = tagfs_create_inode(sb, S_IFLNK | 0755, file_index);
  if (!inode) { return NULL; }
  inode->i_op = inode_ops;
  inode->i_fop = file_ops;
  d_add(owner_de, inode);
  return inode;
}
