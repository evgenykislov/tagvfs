#ifndef TAG_DIR_H
#define TAG_DIR_H

#include <linux/fs.h>
#include <linux/kernel.h>

extern const struct inode_operations tagfs_dir_inode_ops;
extern const struct file_operations tagfs_dir_file_ops;

/*! Добавляет к негативному dentry новый inode
\param sb суперблок
\param owner_de структура dentry, к которой добавляется inode
\param dir_index номер inode-а, который будет добавлен к dentry
\param inode_ops структура с inode операциями inode-а (может быть NULL)
\param file_ops структура с файловыми операциями inode-а (может быть NULL)
\return либо указатель на созданный inode, либо NULL если есть ошибки */
struct inode* tagfs_fills_dentry_by_inode(struct super_block* sb,
    struct dentry* owner_de, size_t dir_index, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops);

int tagfs_dir_iterate(struct file* f, struct dir_context* dc);

void tagfs_printk_dentry(struct dentry* de);


#endif // TAG_DIR_H
