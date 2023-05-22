#include <linux/kernel.h>

#include "tag_dir.h"
#include "tag_inode.h"
#include "tag_storage.h"

/*! Счётчик ноды для директорий. Всегда растёт. Когда заполнится - директории
выводится не будет, придётся перезапускать модуль файловой системы */
atomic_t dirino_counter = ATOMIC_INIT(kFSDirectoriesStartIno);


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


struct inode* create_directory_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dirino,
    const struct inode_operations* inode_ops, const struct file_operations* file_ops) {
  struct inode* nod;

  if (dirino == 0) {
    dirino = atomic_inc_return(&dirino_counter);
    if (dirino < kFSDirectoriesStartIno || dirino > kFSDirectoriesFinishIno) {
      // Выход за допустимый диапазон индексов для директорий
      atomic_set(&dirino_counter, kFSDirectoriesFinishIno);
      return ERR_PTR(-ENFILE);
    }
  }

  nod = tagfs_create_inode(sb, S_IFDIR | 0777, dirino);
  if (!nod) { return ERR_PTR(-ENOMEM); }
  // У созданной ноды уже установлены некоторые дефалтовые операции. Если нам
  // не нужно ставить свои то ничего не меняем
  if (inode_ops) { nod->i_op = inode_ops; }
  if (file_ops) { nod->i_fop = file_ops; }
  return nod;
}


struct inode* fill_lookup_dentry_by_new_directory_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dirino, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops) {
  struct inode* nod;

  nod = create_directory_inode(sb, owner_de, dirino, inode_ops, file_ops);
  if (IS_ERR(nod)) { return nod; }
  d_add(owner_de, nod);
  return nod;
}


struct inode*  fill_dentry_by_new_directory_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dirino,
    const struct inode_operations* inode_ops, const struct file_operations* file_ops) {
  struct inode* nod;

  nod = create_directory_inode(sb, owner_de, dirino, inode_ops, file_ops);
  if (IS_ERR(nod)) { return nod; }
  d_instantiate(owner_de, nod);
  return nod;
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
