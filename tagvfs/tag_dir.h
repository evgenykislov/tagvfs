#ifndef TAG_DIR_H
#define TAG_DIR_H

#include <linux/fs.h>
#include <linux/kernel.h>

extern const struct inode_operations tagfs_dir_inode_ops;
extern const struct file_operations tagfs_dir_file_ops;

int tagfs_init_dir(void);

struct dentry* tagfs_create_dir(struct super_block* sb,
    struct dentry* owner_dir, const struct qstr* dir_name, size_t dir_index);

int tagfs_dir_iterate(struct file* f, struct dir_context* dc);

void tagfs_printk_dentry(struct dentry* de);


#endif // TAG_DIR_H
