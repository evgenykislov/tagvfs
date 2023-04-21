#ifndef TAG_INODE_H
#define TAG_INODE_H

#include <linux/fs.h>
#include <linux/kernel.h>

/*! Создаём ни к чему не привязанный inode
\param sb СуперБлок файловой системы
\param mode Режим файла/сущности. Это и тип, и биты доступа
\param index Индекс файла/сущности в кастомной файловой системе */
struct inode* tagfs_create_inode(struct super_block* sb, umode_t mode,
    size_t index);

void tagfs_printk_inode(const struct inode* ind, unsigned int indent);


#endif // TAG_INODE_H
