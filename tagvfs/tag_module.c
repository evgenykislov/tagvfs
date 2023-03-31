
#include <linux/kernel.h>
#include <linux/module.h>


#include "tag_fs.h"

static int tagvfs_init(void) {
  int res = init_fs();
  if (res != 0) { return res; }

  return 0;
}

static void tagvfs_exit(void) {
  int res = exit_fs();
  if (res != 0) {
    printk(KERN_WARNING "Error in exit of filesystem: %d", res);
  }
}


module_init(tagvfs_init);
module_exit(tagvfs_exit);

MODULE_LICENSE("GPL");
