#ifndef COMMON_H
#define COMMON_H

#include <linux/fs.h>


#define kModuleLogName "tagvfs: "


/*! Хелпер: Получить указатель на хранилище через super_block */
extern void* super_block_storage(const struct super_block* sb);

/*! Хелпер: Получить указатель на хранилище через inode */
extern void* inode_storage(const struct inode* nod);

#endif // COMMON_H
