#include "tag_file.h"

#include <linux/slab.h>

#include "tag_inode.h"
#include "tag_storage.h"

struct file_data {
  size_t ino; //!< Номер файла в файловой системе
  // Data from file
  void* data;
  size_t len;
};

const char kEmptyLink[] = "";


void flink_delay_buffer(void* buf) {
  kfree(buf);
}

const char* flink_getlink(struct dentry* de, struct inode* inode,
    struct delayed_call* delay_call) {
  char* data;
  size_t ino;
  size_t red;
  size_t data_size; //!< Размер данных, выделенный в data без учёта нулевого символа

  if (inode->i_ino < kFSRealFilesStartIno || inode->i_ino > kFSRealFilesFinishIno) {
    return kEmptyLink;
  }
  ino = inode->i_ino - kFSRealFilesStartIno;

  data_size = tagfs_get_file_size(ino);
  if (data_size == 0) {
    return kEmptyLink;
  }

  data = (char*)kzalloc(data_size + 1, GFP_KERNEL);
  if (!data) { return kEmptyLink; }
  red = tagfs_get_file_data(ino, 0, data_size, data);
  if (red != data_size) { goto ea_ad; }
  data[data_size] = '\0';

  set_delayed_call(delay_call, flink_delay_buffer, data);
  return data;

// -------------
ea_ad: // Errors after allocation of data
  kfree(data);
  return kEmptyLink;
}

const static struct file_operations linkfile_fops = {
};

const struct inode_operations linkfile_iops = {
  .get_link = flink_getlink
};

struct inode* tagfs_fills_dentry_by_linkfile_inode(struct super_block* sb,
    struct dentry* owner_de, size_t file_index) {
  struct inode* inode;

  inode = tagfs_create_inode(sb, S_IFLNK | 0755, file_index);
  if (!inode) { return NULL; }

  inode->i_op = &linkfile_iops;
  inode->i_fop = &linkfile_fops;
  d_add(owner_de, inode);
  return inode;
}
