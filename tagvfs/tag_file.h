#ifndef TAG_FILE_H
#define TAG_FILE_H

#include <linux/fs.h>
#include <linux/kernel.h>


/*! Добавляет к негативному dentry новый inode для файла-линка. Операции на файлами
прописаны внутренние (внешние не предполагаются).
\param sb суперблок
\param owner_de структура dentry, к которой добавляется inode
\param file_index номер inode-а, который будет добавлен к dentry
\return либо указатель на созданный inode, либо NULL если есть ошибки */
struct inode* tagfs_fills_dentry_by_linkfile_inode(struct super_block* sb,
    struct dentry* owner_de, size_t file_index);


#endif // TAG_FILE_H
