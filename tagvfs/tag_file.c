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


struct dentry* tagfs_create_file(struct super_block* sb,
    struct dentry* owner_dir, const struct qstr* file_name, size_t file_index) {
  struct dentry* dentry;
  struct inode* inode;

  dentry = d_alloc(owner_dir, file_name);
  inode = tagfs_create_inode(sb, S_IFREG | 0644, file_index);

  if (!inode) {
    return NULL;
  }

  dentry->d_op = &simple_dentry_operations;
  inode->i_fop = &tagfs_file_ops;

  d_add(dentry, inode);
  return dentry;
}
