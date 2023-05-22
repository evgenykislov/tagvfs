#include "tag_tag_dir.h"

#include <linux/slab.h>

#include "common.h"
#include "inode_info.h"
#include "tag_dir.h"
#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"


struct FileInfo {
  // Маркеры для итерации по директории
  loff_t last_iterate_pos; //!< Позиция последней записи (возможно, неуспешной)
  size_t last_iterate_tag; //!< Номер тэга, соответствующий last_iterate_pos. Или kNotFoundIno - если позиция не тэг
  size_t last_iterate_file; //!< Номер файла, соответствующего last_iterate_pos. Или kNotFoundIno - если позиция не файл
};


struct dentry* tagfs_tag_dir_lookup(struct inode* dir, struct dentry *de,
    unsigned int flags) {
  struct inode* inode;
  struct super_block* sb;
  Storage stor;
  size_t tagino, fileino;
  struct InodeInfo* dir_info = get_inode_info(dir);
  struct TagMask mask = tagmask_empty();
  bool mask_suitable;

  sb = dir->i_sb;
  stor = super_block_storage(sb);

  tagino = tagfs_get_tagino_by_name(stor, de->d_name);
  if (tagino != kNotFoundIno) {
    // Имя - это тэг
    struct InodeInfo* iinfo;
    size_t mask_len;

    inode = fill_lookup_dentry_by_new_directory_inode(sb, de, 0, &tagfs_tag_dir_inode_ops,
        &tagfs_tag_dir_file_ops);
    if (IS_ERR(inode)) {
      d_add(de, NULL);
      return NULL;
    }

    iinfo = get_inode_info(inode);
    iinfo->tag_ino = tagino;
    WARN_ON(!tagmask_is_empty(iinfo->on_mask));
    WARN_ON(!tagmask_is_empty(iinfo->off_mask));
    mask_len = tagfs_get_maximum_tags_amount(stor);
    iinfo->on_mask = tagmask_init_by_tag(mask_len, tagino);
    iinfo->off_mask = tagmask_init_zero(mask_len);

    return NULL;
  }

  // Имя - это файл
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
}


/*! Колбэк на создание символьной ссылки в каталоге. В колбэк передаётся негативный
dentry, который нужно дополнить новым inode.
\param dir директория, в которой создаётся новая ссылка
\param de заголовка (негативная) на элемент в директории
\param name путь к целевому файлу (куда указывает ссылка)
\return код ошибки (0 - ошибки нет) */
int tagfs_tag_dir_symlink(struct inode* dir, struct dentry* de, const char* name) {
  struct inode* newnode;
  size_t ino;
  Storage stor = inode_storage(dir);
  struct InodeInfo* dir_info = get_inode_info(dir);
  size_t exist_ino;
  struct TagMask mask;
  int res = 0;

  exist_ino = tagfs_get_fileino_by_name(stor, de->d_name, &mask);
  if (exist_ino != kNotFoundIno) {
    struct qstr target = get_null_qstr();
    struct qstr income = alloc_qstr_from_str(name, strlen(name)); // TODO PERFORMANCE. REMOVE allocation
    target = tagfs_get_file_link(stor, exist_ino);
    if (!income.name) {
      res = -ENOMEM;
      goto exist_err;
    }

    if (compare_qstr(target, income) != 0) {
      // Попытка создать символьную ссылку с существующим именем, указывающую на
      // другой файл. Говорим, что такой файл уже есть
      res = -EEXIST;
      goto exist_err;
    }

    // Это либо копирование нашей же ссылки из папки в папку, либо создание ссылки
    // на тот же самый файл. В любом случае операцию делаем и двигаем маску тэгов.
    tagmask_or_mask(mask, dir_info->on_mask);
    tagmask_exclude_mask(mask, dir_info->off_mask);
    res = tagfs_set_file_mask(stor, exist_ino, mask);
    tagmask_release(&mask);
    WARN_ON(de->d_inode);
    newnode = tagfs_create_inode(dir->i_sb, S_IFLNK | 0777, exist_ino + kFSRealFilesStartIno);
    if (!newnode) {
      res = -ENOMEM;
      goto exist_err;
    }
    d_instantiate(de, newnode);
    // -------
exist_err:
    free_qstr(&income);
    free_qstr(&target);
    tagmask_release(&mask);
    return res;
  }

  // Создаём новый файл
  ino = tagfs_add_new_file(stor, name, de->d_name);
  newnode = tagfs_create_inode(dir->i_sb, S_IFLNK | 0777, ino + kFSRealFilesStartIno);
  if (!newnode) {
    res = -ENOMEM;
    goto new_err;
  }
  tagfs_set_linkfile_operations_for_inode(newnode);
  d_instantiate(de, newnode);
  // ---------------
new_err:
  tagmask_release(&mask);
  return 0;
}


int tagfs_tag_dir_unlink(struct inode* dir, struct dentry* de) {
  struct InodeInfo* dir_info = get_inode_info(dir);
  Storage stor = inode_storage(dir);
  struct TagMask mask = tagmask_empty();
  struct qstr name = get_null_qstr();
  int res;
  size_t fileino;
  struct inode* fi = de->d_inode;

  if (!fi) { return -ENOENT; }

  if (fi->i_ino < kFSRealFilesStartIno || fi->i_ino > kFSRealFilesFinishIno) {
    return -EINVAL;
  }
  fileino = fi->i_ino - kFSRealFilesStartIno;
  name = tagfs_get_fname_by_ino(stor, fileino, &mask);
  if (!name.name) {
    // Файл не существует
    return -ENOENT;
  }
  free_qstr(&name);

  tagmask_set_tag(mask, dir_info->tag_ino, false);
  res = tagfs_set_file_mask(stor, fileino, mask);
  tagmask_release(&mask);
  return res;
}


int tagfs_tag_dir_mkdir(struct inode* dir,struct dentry* de, umode_t mode) {
  size_t tagino;
  int res;
  struct inode* newnode;
  struct InodeInfo* dir_info = get_inode_info(dir);
  struct InodeInfo* new_info;
  size_t mask_len;
  struct super_block* sb = dir->i_sb;

  Storage stor = inode_storage(dir);
  res = tagfs_add_new_tag(stor, de->d_name, &tagino); // TODO CHECK EXISTANCE
  if (res) { return res; }

  newnode = fill_dentry_by_new_directory_inode(sb, de, 0, &tagfs_tag_dir_inode_ops,
      &tagfs_tag_dir_file_ops);
  if (IS_ERR(newnode)) { return PTR_ERR(newnode); }

  new_info = get_inode_info(newnode);
  new_info->tag_ino = tagino;
  WARN_ON(!tagmask_is_empty(new_info->on_mask));
  WARN_ON(!tagmask_is_empty(new_info->off_mask));
  mask_len = tagfs_get_maximum_tags_amount(stor);
  new_info->on_mask = tagmask_init_by_tag(mask_len, tagino);
  new_info->off_mask = tagmask_init_zero(mask_len);
  tagmask_or_mask(new_info->on_mask, dir_info->on_mask);
  tagmask_or_mask(new_info->off_mask, dir_info->off_mask);
  return 0;
}

int tagfs_tag_dir_iterate(struct file* f, struct dir_context* dc) {
  size_t ino;
  Storage stor;
  struct qstr name;
  bool tags_emit = false;
  size_t file_start;
  struct InodeInfo* iinfo;
  size_t tags_amount;

  stor = inode_storage(file_inode(f));
  iinfo = get_inode_info(file_inode(f));
  BUG_ON(!stor);

  if (!dir_emit_dots(f, dc)) { return -ENOMEM; }

  tags_amount = tagfs_get_active_tags_amount(stor);

  // Выдадим все тэги с заданного номера. "-2" для учёта директорий "." и ".."
  if (dc->pos < (tags_amount + 2)) {
    for (name = tagfs_get_nth_tag(stor, dc->pos - 2, &ino);
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
  }

  file_start = 0;
  if (!tags_emit) {
    WARN_ON(dc->pos < (tags_amount + 2));
    file_start = dc->pos - tags_amount - 2;
  }

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

int tagfs_tag_dir_open(struct inode* dir, struct file* f) {
  struct FileInfo* fi;

  fi = (struct FileInfo*)kzalloc(sizeof(struct FileInfo), GFP_KERNEL);
  if (fi == NULL) { return -ENOMEM; }
  f->private_data = fi;
  fi->last_iterate_pos = -1;
  fi->last_iterate_tag = kNotFoundIno;
  fi->last_iterate_file = kNotFoundIno;

  return 0;
}

int tagfs_tag_dir_release(struct inode* dir, struct file* f) {
  WARN_ON(!f->private_data);
  kfree(f->private_data);
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
  .symlink = tagfs_tag_dir_symlink,
  .unlink = tagfs_tag_dir_unlink,
  .mkdir = tagfs_tag_dir_mkdir
};

const struct file_operations tagfs_tag_dir_file_ops = {
  .open = tagfs_tag_dir_open,
  .release = tagfs_tag_dir_release,
  .llseek = tagfs_tag_dir_llseek,
  .read = generic_read_dir,
  .iterate_shared = tagfs_tag_dir_iterate
};
