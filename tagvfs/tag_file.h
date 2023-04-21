#ifndef TAG_FILE_H
#define TAG_FILE_H

#include <linux/fs.h>
#include <linux/kernel.h>


/*! Созаёт запись (dentry) про файл
\param sb
\param owner_dir
\param file_name
\param file_number */
struct dentry* tagfs_create_file(struct super_block* sb,
    struct dentry* owner_dir, const struct qstr* file_name, size_t file_number);


#endif // TAG_FILE_H
