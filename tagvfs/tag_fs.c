#include "tag_fs.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "tag_dir.h"
#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"


const char tagvfs_name[] = "tagvfs";

const size_t kRootIndex = 2;
const unsigned long kMagicTag = 0x34562343; //!< Магическое число для идентификации файловой системы


int compare_qstr(unsigned int len, const char* str, const struct qstr* name) {
  if (len != name->len) {
    return 1;
  }
  return !!memcmp(str, name->name, len);
}


struct qstr kDirName = QSTR_INIT("d01", 3);
const size_t kDirIndex = 14;
struct qstr kFileName = QSTR_INIT("f01", 3);
const size_t kFileIndex = 12;


int tagfs_root_iterate(struct file* f, struct dir_context* dc) {
  if (!dir_emit_dots(f, dc)) { return 0; }

  if (dc->pos == 2) {
    dir_emit(dc, kDirName.name, kDirName.len, kDirIndex, DT_DIR);
    dc->pos += 1;
  }

  if (dc->pos == 3) {
    dir_emit(dc, kFileName.name, kFileName.len, kFileIndex, DT_REG);
    dc->pos += 1;
  }

  return 0;
}

int tagfs_root_open(struct inode *inode, struct file *file) {
  return 0;
}

int tagfs_root_release(struct inode *inode, struct file *file) {
  return 0;
}

int tagfs_root_dinit(struct dentry* de) {
  return 0;
}

struct dentry* tagfs_root_lookup(struct inode* parent_i, struct dentry* de,
    unsigned int flags) {
  struct inode* inode;

  if (compare_qstr(de->d_name.len, de->d_name.name, &kDirName) == 0) {
    inode = tagfs_create_inode(parent_i->i_sb, S_IFDIR | 0755, kDirIndex);
    inode->i_fop = &tagfs_dir_file_ops;
    inode->i_op = &tagfs_dir_inode_ops;
    d_add(de, inode);
    return NULL;
  }

  if (compare_qstr(de->d_name.len, de->d_name.name, &kFileName) == 0) {
    inode = tagfs_create_inode(parent_i->i_sb, S_IFREG | 0755, kFileIndex);
    d_add(de, inode);
    return NULL;
  }

  return ERR_PTR(-ENOENT);
}


const struct file_operations tagfs_root_file_ops = {
  .open = tagfs_root_open,
  .release = tagfs_root_release,
  .iterate = tagfs_root_iterate
};

struct inode_operations tagfs_root_inode_ops = {
  .lookup = tagfs_root_lookup
};


static const struct super_operations tagfs_ops = {
  .statfs = simple_statfs,
  .drop_inode = generic_delete_inode
};


static const struct dentry_operations tagfs_common_dentry_ops = {
  .d_init = tagfs_root_dinit
};


static int fs_fill_superblock(struct super_block* sb, void* data, int silent) {
  struct inode* root_inode;

  sb->s_blocksize = PAGE_SIZE;
  sb->s_blocksize_bits = PAGE_SHIFT;
  sb->s_maxbytes = LLONG_MAX;
  sb->s_magic = kMagicTag;
  sb->s_op = &tagfs_ops;
  sb->s_d_op = &tagfs_common_dentry_ops;

  // Create root inode
  root_inode = tagfs_create_inode(sb, S_IFDIR | 0755, kRootIndex);
  if (!root_inode) { return -ENOMEM; }
  root_inode->i_fop = &tagfs_root_file_ops;
  root_inode->i_op = &tagfs_root_inode_ops;

  sb->s_root = d_make_root(root_inode);
  if (!sb->s_root) { return -ENOMEM; }

  return 0;
}


struct dentry* fs_mount(struct file_system_type* fstype, int flags,
    const char* dev_name, void* data) {
  return mount_nodev(fstype, flags, data, fs_fill_superblock);
}

struct file_system_type fs_type = { .name = tagvfs_name, .mount = fs_mount,
    .kill_sb = generic_shutdown_super, .owner = THIS_MODULE, .next = NULL};

int init_fs(void) {
  return register_filesystem(&fs_type);
}


int exit_fs(void) {
  return unregister_filesystem(&fs_type);
}
