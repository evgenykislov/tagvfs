#include "tag_allfiles_dir.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "tag_dir.h"
#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"

struct dir_data {
  unsigned long last_iterate_ino;
};


int tagfs_allfiles_dir_iterate(struct file* f, struct dir_context* dc) {
  struct dir_data* dd;
  size_t counter;
  size_t ino;

  if (!dir_emit_dots(f, dc)) { return -ENOMEM; }

  counter = 2;
  ino = 0;
  dd = (struct dir_data*)(f->private_data);
  while (true) {
    size_t fino;
    struct qstr name;
    bool skip;

    skip = counter < dc->pos ? true : false;

    tagfs_get_first_name(ino, NULL /* TODO */, &fino, &name);
    if (fino == kNotFoundIno) {
      break;
    }

    ino = fino + 1;
    ++counter;

    if (!skip) {
      if (!dir_emit(dc, name.name, name.len, fino + kFSRealFilesStartIno, DT_LNK)) {
        return -ENOMEM;
      }
      dc->pos += 1;
    }
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

struct dentry* tagfs_allfiles_dir_lookup(struct inode* dir, struct dentry *de,
    unsigned int flags) {
  size_t ino;
  struct inode* inode;
  struct super_block* sb;

  ino = tagfs_get_ino_of_name(de->d_name);
  if (ino == kNotFoundIno) {
    d_add(de, NULL);
    return NULL;
  }
  sb = dir->i_sb;

  inode = tagfs_fills_dentry_by_linkfile_inode(sb, de, ino + kFSRealFilesStartIno);
  if (!inode) { return ERR_PTR(-ENOMEM); }
  return NULL;
}

int tagfs_allfiles_dir_symlink(struct inode* inod, struct dentry* de, const char* name) {
  struct inode* newnode;
  size_t ino;

  ino = tagfs_add_new_file(name, de->d_name);
  newnode = tagfs_create_inode(inod->i_sb, S_IFLNK | 0777, ino + kFSRealFilesStartIno);
  if (!newnode) {
    return -ENOMEM;
  }
  tagfs_set_linkfile_operations_for_inode(newnode);
  d_instantiate(de, newnode);

  return 0;
}

loff_t tagfs_allfiles_dir_llseek(struct file* f, loff_t offset, int whence) {
  switch (whence) {
    case 0: // absolute offset
      f->f_pos = offset;
      break;
    case 1: // relative offset
      f->f_pos += offset;
      break;
    default:
      f->f_pos = -1;
  }

  if (f->f_pos >= 0) {
    return f->f_pos;
  }
  return -EINVAL;
}


const struct inode_operations tagfs_allfiles_dir_inode_ops = {
  .lookup = tagfs_allfiles_dir_lookup,
  .symlink = tagfs_allfiles_dir_symlink
};


const struct file_operations tagfs_allfiles_dir_file_ops = {
  .open = tagfs_allfiles_dir_open,
  .release = tagfs_allfiles_dir_release,
  .llseek = tagfs_allfiles_dir_llseek,
  .read = generic_read_dir,
  .iterate_shared = tagfs_allfiles_dir_iterate
};
