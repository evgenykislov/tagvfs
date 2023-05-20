#include "tag_file.h"

#include <linux/slab.h>

#include "common.h"
#include "tag_inode.h"
#include "tag_storage.h"

const char kEmptyLink[] = "";


void flink_delay_buffer(void* buf) {
  kfree(buf);
}

const char* flink_getlink(struct dentry* de, struct inode* inode,
    struct delayed_call* delay_call) {
  char* data;
  size_t ino;
  struct qstr lnk = QSTR_INIT(NULL, 0);
  Storage stor = inode_storage(inode);
  const char* res = kEmptyLink;

  if (inode->i_ino < kFSRealFilesStartIno || inode->i_ino > kFSRealFilesFinishIno) {
    return kEmptyLink;
  }
  ino = inode->i_ino - kFSRealFilesStartIno;

  lnk = tagfs_get_file_link(stor, ino);
  if (lnk.len == 0) {
    return kEmptyLink;
  }

  data = (char*)kzalloc(lnk.len + 1, GFP_KERNEL);
  if (!data) { goto ea_ad; }
  memcpy(data, lnk.name, lnk.len);
  data[lnk.len] = '\0';
  free_qstr(&lnk);

  set_delayed_call(delay_call, flink_delay_buffer, data);

  return data;

// -------------
ea_ad: // Errors after allocation of data
  free_qstr(&lnk);
  kfree(data);
  return res;
}

const static struct file_operations linkfile_fops = {
};

const struct inode_operations linkfile_iops = {
  .get_link = flink_getlink
};

struct inode* tagfs_fills_dentry_by_linkfile_inode(struct super_block* sb,
    struct dentry* owner_de, size_t file_index) {
  struct inode* inode;

  inode = tagfs_create_inode(sb, S_IFLNK | 0777, file_index);
  if (!inode) { return NULL; }

  inode->i_op = &linkfile_iops;
  inode->i_fop = &linkfile_fops;
  d_add(owner_de, inode);
  return inode;
}

void tagfs_set_linkfile_operations_for_inode(struct inode* nod) {
  if (!nod) { return; }

  nod->i_op = &linkfile_iops;
  nod->i_fop = &linkfile_fops;
}
