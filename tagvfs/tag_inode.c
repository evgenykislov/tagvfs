#include "tag_inode.h"

#include <linux/slab.h>

#include "tag_dir.h"

/* Структура inode-а для тэговой файловой системы */
struct TagfsInode {
  struct inode nod;
  struct InodeInfo info;
};


const size_t kInodeSize = 1; //!< Размер файла (символьной ссылки) для файловых менеджеров


struct inode* tagfs_create_inode(struct super_block* sb, umode_t mode,
    size_t index) {
  struct inode* n = new_inode(sb);
  if (!n) {
    return NULL;
  }

  n->i_mode = mode;
  n->i_uid.val = n->i_gid.val = 0;
  n->i_size = PAGE_SIZE;
  n->i_blocks = 0;
  n->i_size = kInodeSize;
  n->i_ino = index;
  n->i_atime = n->i_ctime = n->i_mtime = current_time(n);
  switch (mode & S_IFMT) {
    case S_IFDIR:
      set_nlink(n, 2);
      break;
    case S_IFLNK:
      break;
  }

  return n;
}


struct inode* tagfs_inode_alloc(struct super_block *sb) {
  struct TagfsInode* d;

  d = kzalloc(sizeof(struct TagfsInode), GFP_KERNEL);
  if (!d) { return NULL; }

  // Обязательно инициализировать, иначе при размонтировании будет крэш
  inode_init_once(&(d->nod));

  d->info.tag_ino = 0;
  d->info.on_mask = tagmask_empty();
  d->info.off_mask = tagmask_empty();

  return &(d->nod);
}


void tagfs_inode_free(struct inode* nod) {
  struct TagfsInode* d;

  d = container_of(nod, struct TagfsInode, nod);

  tagmask_release(&(d->info.on_mask));
  tagmask_release(&(d->info.off_mask));

  kfree(d);
}


struct InodeInfo* get_inode_info(struct inode* nod) {
  struct TagfsInode* d = container_of(nod, struct TagfsInode, nod);
  return &(d->info);
}

void tagfs_printk_inode(const struct inode* ind, unsigned int indent) {
  #define kIndentStrSize 10
  char ind_str[kIndentStrSize + 1] = "          ";
  if (indent < kIndentStrSize) {
    ind_str[indent] = '\0';
  }

  pr_info("%sinode: %px\n", ind_str, ind);
  if (!ind) {
    pr_info("%s  NULL\n", ind_str);
    return;
  }
  pr_info("%s  i_mode: \\0%o\n", ind_str, (unsigned int)ind->i_mode);
  pr_info("%s  i_ino: %ld\n", ind_str, ind->i_ino);
  pr_info("%s  i_nlink: %x\n", ind_str, ind->i_nlink);
  pr_info("%s  i_count: %x\n", ind_str, atomic_read(&ind->i_count));
  pr_info("%s  i_sb: %p\n", ind_str, ind->i_sb);
}
