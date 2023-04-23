#include "tag_allfiles_dir.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"

struct dir_data {
  unsigned long last_iterate_ino;
};


int tagfs_allfiles_dir_iterate(struct file* f, struct dir_context* dc) {
  struct dir_data* dd;

  if (!dir_emit_dots(f, dc)) { return -ENOMEM; }

  dd = (struct dir_data*)(f->private_data);
  while (true) {
    size_t fino;
    struct qstr name;

    tagfs_get_first_name(dd->last_iterate_ino, NULL /* TODO */, &fino, &name);
    if (fino == kNotFoundIno) {
      return 0;
    }
    dd->last_iterate_ino = fino + 1;

    dir_emit(dc, name.name, name.len, fino + kFSRealFilesStartIno, DT_LNK);
    dc->pos += 1;
  }
  return 0;
}

int tagfs_allfiles_dir_open(struct inode *inode, struct file *file) {
  struct dir_data* dd;

  file->private_data = kzalloc(sizeof(struct dir_data), GFP_KERNEL);
  if (!file->private_data) { return -ENOMEM; }

  dd = (struct dir_data*)(file->private_data);
  dd->last_iterate_ino = 0;
  return 0;
}

int tagfs_allfiles_dir_release(struct inode *inode, struct file *file) {
  kfree(file->private_data);
  file->private_data = NULL;
  return 0;
}

ssize_t tagfs_allfiles_dir_read(struct file* filp, char __user* buf, size_t siz, loff_t* ppos) {
  printk(KERN_INFO "TODO dir read function\n");
  return -EISDIR;
}


struct dentry* tagfs_allfiles_dir_lookup(struct inode* dir, struct dentry *de,
    unsigned int flags) {
  size_t ino;
  struct inode* inode;
  struct super_block* sb;

  ino = tagfs_get_ino_of_name(de->d_name);
  if (ino == kNotFoundIno) { return ERR_PTR(-ENOENT); }
  sb = dir->i_sb;

  inode = tagfs_fills_dentry_by_linkfile_inode(sb, de, ino + kFSRealFilesStartIno);
  if (!inode) { return ERR_PTR(-ENOMEM); }
  return NULL;
}

const struct inode_operations tagfs_allfiles_dir_inode_ops = {
  .lookup = tagfs_allfiles_dir_lookup
};


const struct file_operations tagfs_allfiles_dir_file_ops = {
  .open = tagfs_allfiles_dir_open,
  .release = tagfs_allfiles_dir_release,
  .read = tagfs_allfiles_dir_read,
  .iterate_shared = tagfs_allfiles_dir_iterate
};
