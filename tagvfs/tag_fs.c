#include "tag_fs.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>


const char tagvfs_name[] = "tagvfs";


struct dentry* fs_mount(struct file_system_type* fstype, int flags,
    const char* dev_name, void* data) {
  return NULL;
}

void fs_kill(struct super_block* fs_sb) {

}

struct file_system_type fs_type = { .name = tagvfs_name, .mount = fs_mount,
    .kill_sb = fs_kill, .owner = THIS_MODULE};

int init_fs(void) {




  return 0;
}


int exit_fs(void) {
  return -1;
}
