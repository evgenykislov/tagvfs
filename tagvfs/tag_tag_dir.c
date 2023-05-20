#include "tag_tag_dir.h"

#include "common.h"
#include "inode_info.h"
#include "tag_dir.h"
#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"

/*! Счётчик ноды для директорий. Всегда растёт. Когда заполнится - директории
выводится не будет, придётся перезапускать модуль файловой системы */
atomic_t dirino_counter = ATOMIC_INIT(kFSDirectoriesStartIno);


struct dentry* tagfs_tag_dir_lookup(struct inode* dir, struct dentry *de,
    unsigned int flags) {
  struct inode* inode;
  struct super_block* sb;
  Storage stor;
  u32 dirino;
  size_t tagino, fileino;
  struct dentry* res = NULL;
  struct InodeInfo* dir_info = get_inode_info(dir);
  struct TagMask mask = tagmask_empty();
  bool mask_suitable;

  sb = dir->i_sb;
  stor = super_block_storage(sb);

  tagino = tagfs_get_tagino_by_name(stor, de->d_name);
  if (tagino != kNotFoundIno) {
    // Имя - это не тэг
    struct InodeInfo* iinfo;

    size_t mask_len = tagfs_get_maximum_tags_amount(stor);

    dirino = atomic_inc_return(&dirino_counter);
    if (dirino < kFSDirectoriesStartIno || dirino > kFSDirectoriesFinishIno) {
      // Выход за допустимый диапазон индексов для директорий
      atomic_set(&dirino_counter,kFSDirectoriesFinishIno);
      res = ERR_PTR(-ENOMEM);
      goto err;
    }

    inode = tagfs_fills_dentry_by_inode(sb, de, dirino, &tagfs_tag_dir_inode_ops,
        &tagfs_tag_dir_file_ops);
    if (!inode) { return ERR_PTR(-ENOMEM); }

    iinfo = get_inode_info(inode);
    iinfo->tag_ino = tagino;
    WARN_ON(!tagmask_is_empty(iinfo->on_mask));
    WARN_ON(!tagmask_is_empty(iinfo->off_mask));
    iinfo->on_mask = tagmask_init_by_tag(mask_len, tagino);
    iinfo->off_mask = tagmask_init_zero(mask_len);

    return NULL;
  }

  // Имя - это тэг
  fileino = tagfs_get_fileino_by_name(stor, de->d_name, &mask);
  mask_suitable = tagmask_check_filter(mask, dir_info->on_mask,
      dir_info->off_mask);
  tagmask_release(&mask);
  if (!mask_suitable || fileino == kNotFoundIno) {
    d_add(de, NULL);
    return NULL;
  }

  inode = tagfs_fills_dentry_by_linkfile_inode(sb, de, fileino + kFSRealFilesStartIno);
  if (!inode) { return ERR_PTR(-ENOMEM); }
  return NULL;

  // --------------
err:
  d_add(de, NULL);
  return res;
}



int tagfs_tag_dir_symlink(struct inode* inod, struct dentry* de, const char* name)
// de содержит имя ссылки
// name содержит путь к целевому файлу
{
  struct inode* newnode;
  size_t ino;
  Storage stor = inode_storage(inod);
  struct InodeInfo* dir_info = get_inode_info(inod);

  if (de->d_sb == inod->i_sb) {
    // Копирование симлинков в пределах одной файловой системы
    // Меняем тэги на файле
    struct TagMask mask = tagmask_empty();
    size_t fileino;
    int res;

    fileino = tagfs_get_fileino_by_name(stor, de->d_name, &mask);
    if (fileino == kNotFoundIno) { return -EFAULT; }
    tagmask_or_mask(mask, dir_info->on_mask);
    tagmask_exclude_mask(mask, dir_info->off_mask);
    res = tagfs_set_file_mask(stor, fileino, mask);
    tagmask_release(&mask);
    return res;
  }

  // Создаём новый файл
  ino = tagfs_add_new_file(stor, name, de->d_name);
  newnode = tagfs_create_inode(inod->i_sb, S_IFLNK | 0777, ino + kFSRealFilesStartIno);
  if (!newnode) { return -ENOMEM; }
  tagfs_set_linkfile_operations_for_inode(newnode);
  d_instantiate(de, newnode);

  return 0;
}



int tagfs_tag_dir_iterate(struct file* f, struct dir_context* dc) {
  size_t ino;
  Storage stor;
  struct qstr name;
  size_t tags_proc;
  bool tags_emit = false;
  size_t file_start;
  struct InodeInfo* iinfo;

  stor = inode_storage(file_inode(f));
  BUG_ON(!stor);

  if (!dir_emit_dots(f, dc)) { return -ENOMEM; }

  // Выдадим все тэги с заданного номера. "-2" для учёта директорий "." и ".."
  for (name = tagfs_get_nth_tag(stor, dc->pos - 2, &ino, &tags_proc);
      ino != kNotFoundIno; name = tagfs_get_next_tag(stor, &ino)) {
    if (!dir_emit(dc, name.name, name.len, ino + kFSRealFilesStartIno,
        DT_LNK)) {
      free_qstr(&name);
      return -ENOMEM;
    }
    tags_emit = true;
    dc->pos += 1;
    free_qstr(&name);
  }

  file_start = 0;
  if (!tags_emit) {
    if (dc->pos >= (tags_proc + 2)) {
      file_start = dc->pos - tags_proc - 2;
    } else {
      WARN_ON(1);
    }
  }

  iinfo = get_inode_info(file_inode(f));
  for (name = tagfs_get_nth_file(stor, iinfo->on_mask, iinfo->off_mask,
      file_start, &ino); ino != kNotFoundIno; name = tagfs_get_next_file(stor,
      iinfo->on_mask, iinfo->off_mask, &ino)) {
    if (ino == kNotFoundIno) {
      break;
    }

    if (!dir_emit(dc, name.name, name.len, ino + kFSRealFilesStartIno, DT_LNK)) {
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
  .lookup = tagfs_tag_dir_lookup,
  .symlink = tagfs_tag_dir_symlink
};

const struct file_operations tagfs_tag_dir_file_ops = {
  .llseek = tagfs_tag_dir_llseek,
  .read = generic_read_dir,
  .iterate_shared = tagfs_tag_dir_iterate
};
