#include "tag_fs.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "common.h"
#include "tag_allfiles_dir.h"
#include "tag_dir.h"
#include "tag_file.h"
#include "tag_inode.h"
#include "tag_storage.h"


const char tagvfs_name[] = "tagvfs";

static const size_t kRootIndex = kFSSpecialNameStartIno + 1;
static const size_t kAllFilesIndex = kFSSpecialNameStartIno + 2;
static const size_t kFilesWOTagsIndex = kFSSpecialNameStartIno + 3;
static const size_t kTagsIndex = kFSSpecialNameStartIno + 4;
static const size_t kControlIndex = kFSSpecialNameStartIno + 5;

const unsigned long kMagicTag = 0x34562343; //!< Магическое число для идентификации файловой системы


int compare_qstr(unsigned int len, const char* str, const struct qstr* name) {
  if (len != name->len) {
    return 1;
  }
  return !!memcmp(str, name->name, len);
}


int tagfs_root_iterate(struct file* f, struct dir_context* dc) {
  struct qstr name;
  Storage stor = inode_storage(file_inode(f));

  if (!dir_emit_dots(f, dc)) { return -ENOMEM; }

  // Special files
  if (dc->pos == 2) {
    name = tagfs_get_special_name(stor, kFSSpecialNameAllFiles);
    if (name.len == 0) { return -ENOMEM; }
    if (!dir_emit(dc, name.name, name.len, kAllFilesIndex, DT_DIR)) {
      return -ENOMEM;
    }
    dc->pos += 1;
  }
  if (dc->pos == 3) {
    name = tagfs_get_special_name(stor, kFSSpecialNameFilesWOTags);
    if (name.len == 0) { return -ENOMEM; }
    if (!dir_emit(dc, name.name, name.len, kFilesWOTagsIndex, DT_DIR)) {
      return -ENOMEM;
    }
    dc->pos += 1;
  }
  if (dc->pos == 4) {
    name = tagfs_get_special_name(stor, kFSSpecialNameTags);
    if (name.len == 0) { return -ENOMEM; }
    if (!dir_emit(dc, name.name, name.len, kTagsIndex, DT_DIR)) {
      return -ENOMEM;
    }
    dc->pos += 1;
  }
  if (dc->pos == 5) {
    name = tagfs_get_special_name(stor, kFSSpecialNameControl);
    if (name.len == 0) { return -ENOMEM; }
    if (!dir_emit(dc, name.name, name.len, kControlIndex, DT_REG)) {
      return -ENOMEM;
    }
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
  enum FSSpecialName nt;
  struct super_block* sb = parent_i->i_sb;
  Storage stor = super_block_storage(sb);

  nt = tagfs_get_special_type(stor, de->d_name);
  switch (nt) {
    case kFSSpecialNameAllFiles:
      if (!tagfs_fills_dentry_by_inode(sb, de, kAllFilesIndex,
          &tagfs_allfiles_dir_inode_ops,
          &tagfs_allfiles_dir_file_ops)) { return ERR_PTR(-ENOENT); }
      return NULL;
      break;
    case kFSSpecialNameFilesWOTags:
      if (!tagfs_fills_dentry_by_inode(sb, de, kFilesWOTagsIndex,
          &tagfs_dir_inode_ops,
          &tagfs_dir_file_ops)) { return ERR_PTR(-ENOENT); }
      return NULL;
      break;
    case kFSSpecialNameTags:
      if (!tagfs_fills_dentry_by_inode(sb, de, kTagsIndex, &tagfs_dir_inode_ops,
          &tagfs_dir_file_ops)) { return ERR_PTR(-ENOENT); }
      return NULL;
      break;
    case kFSSpecialNameControl:
      inode = tagfs_create_inode(sb, S_IFREG | 0755, kControlIndex);
      d_add(de, inode);
      return NULL;
      break;
    default:;
  }

  d_add(de, NULL);
  return NULL;
}


loff_t tagfs_root_llseek(struct file* f, loff_t offset, int whence) {
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

const struct file_operations tagfs_root_file_ops = {
  .open = tagfs_root_open,
  .release = tagfs_root_release,
  .iterate = tagfs_root_iterate,
  .llseek = tagfs_root_llseek
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

  // data is a initialized storage pointer
  if (!data) {
    pr_err(kModuleLogName "Logic error at %s:%i\n", __FILE__, __LINE__);
    return -ENOMEM;
  }

  sb->s_blocksize = PAGE_SIZE;
  sb->s_blocksize_bits = PAGE_SHIFT;
  sb->s_maxbytes = LLONG_MAX;
  sb->s_magic = kMagicTag;
  sb->s_op = &tagfs_ops;
  sb->s_d_op = &tagfs_common_dentry_ops;
  sb->s_fs_info = data;

  // Create root inode
  root_inode = tagfs_create_inode(sb, S_IFDIR | 0777, kRootIndex);
  if (!root_inode) { return -ENOMEM; }
  root_inode->i_fop = &tagfs_root_file_ops;
  root_inode->i_op = &tagfs_root_inode_ops;

  sb->s_root = d_make_root(root_inode);
  if (!sb->s_root) { return -ENOMEM; }

  return 0;
}


struct dentry* fs_mount(struct file_system_type* fstype, int flags,
    const char* dev_name, void* data) {
  Storage stor = NULL;
  int res;

  res = tagfs_init_storage(&stor, dev_name);
  if (res) { return ERR_PTR(res); }
  return mount_nodev(fstype, flags, stor, fs_fill_superblock);
}

// TODO Check invoke
void fs_kill(struct super_block* sb) {
  tagfs_release_storage(&sb->s_fs_info);
  generic_shutdown_super(sb);
}

struct file_system_type fs_type = { .name = tagvfs_name, .mount = fs_mount,
    .kill_sb = fs_kill, .owner = THIS_MODULE, .next = NULL};

int init_fs(void) {
  return register_filesystem(&fs_type);
}


int exit_fs(void) {
  return unregister_filesystem(&fs_type);
}
