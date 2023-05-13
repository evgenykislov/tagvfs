#include "tag_tag_dir.h"

#include "common.h"
#include "tag_dir.h"
#include "tag_storage.h"


atomic_t dirino_counter = ATOMIC_INIT(kFSDirectoriesStartIno);


struct dentry* tagfs_tag_dir_lookup(struct inode* dir, struct dentry *de,
    unsigned int flags) {
  size_t ino;
  struct inode* inode;
  struct super_block* sb;
  Storage stor;
  u32 dirino;
  size_t tagino;
  struct dentry* res = NULL;

  dirino = atomic_inc_return(&dirino_counter);
  if (dirino < kFSDirectoriesStartIno || dirino > kFSDirectoriesFinishIno) {
    // Выход за допустимый диапазон индексов для директорий
    atomic_set(&dirino_counter,kFSDirectoriesFinishIno);
    res = ERR_PTR(-ENOMEM);
    goto err;
  }

  sb = dir->i_sb;
  stor = super_block_storage(sb);
  tagino = tagfs_get_tagino_by_name(stor, de->d_name);
  if (ino == kNotFoundIno) {
    goto err;
  }

  inode = tagfs_fills_dentry_by_inode(sb, de, dirino, &tagfs_dir_inode_ops,
      &tagfs_dir_file_ops);
  if (!inode) { return ERR_PTR(-ENOMEM); }
  return NULL;
  // --------------
err:
  d_add(de, NULL);
  return res;
}


int tagfs_tag_dir_iterate(struct file* f, struct dir_context* dc) {
  size_t ino;
  Storage stor;
  struct qstr name;

  stor = inode_storage(file_inode(f));

  if (!dir_emit_dots(f, dc)) { return -ENOMEM; }

  // Выдадим все тэги с заданного номера. "-2" для учёта директорий "." и ".."
  for (name = tagfs_get_nth_tag(stor, dc->pos - 2, &ino); ino != kNotFoundIno;
      name = tagfs_get_next_tag(stor, &ino)) {
    if (!dir_emit(dc, name.name, name.len, ino + kFSRealFilesStartIno,
        DT_LNK)) {
      free_qstr(&name);
      return -ENOMEM;
    }
    dc->pos += 1;
    free_qstr(&name);
  }

  return 0;
}


loff_t tagfs_tag_dir_llseek(struct file* f, loff_t offset, int whence) {
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


const struct inode_operations tagfs_tag_dir_inode_ops = {
  .lookup = tagfs_tag_dir_lookup  /* ,
  .symlink = tagfs_tag_dir_symlink,
  .mkdir = tagfs_tag_dir_mkdir  */
};

const struct file_operations tagfs_tag_dir_file_ops = {
/*  .open = tagfs_tag_dir_open,
  .release = tagfs_tag_dir_release, */
  .llseek = tagfs_tag_dir_llseek,
  .read = generic_read_dir,
  .iterate_shared = tagfs_tag_dir_iterate
};
