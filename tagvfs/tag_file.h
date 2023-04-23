#ifndef TAG_FILE_H
#define TAG_FILE_H

#include <linux/fs.h>
#include <linux/kernel.h>


/*! Добавляет к негативному dentry новый inode для файла-линка
\param sb суперблок
\param owner_de структура dentry, к которой добавляется inode
\param file_index номер inode-а, который будет добавлен к dentry
\param inode_ops структура с inode операциями inode-а (может быть NULL)
\param file_ops структура с файловыми операциями inode-а (может быть NULL)
\return либо указатель на созданный inode, либо NULL если есть ошибки */
struct inode* tagfs_fills_file_dentry_by_inode(struct super_block* sb,
    struct dentry* owner_de, size_t file_index, const struct inode_operations* inode_ops,
    const struct file_operations* file_ops);


#endif // TAG_FILE_H
