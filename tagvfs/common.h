#ifndef COMMON_H
#define COMMON_H

#include <linux/fs.h>

#define kModuleLogName "tagvfs: "


/*! Хелпер: Получить указатель на хранилище через super_block */
extern void* super_block_storage(const struct super_block* sb);

/*! Хелпер: Получить указатель на хранилище через inode */
extern void* inode_storage(const struct inode* nod);

/*! Создаёт новую строку и заполняет её данными
\param str строка, которую скопировать в результат
\param len длина строки
\return структура qstr с копией сторочных данных */
extern struct qstr alloc_qstr_from_str(const char* str, size_t len);

/*! Создаёт новую строку и заполняет её данными из исходного аргумента.
Созданную строку нужно потом явно удалить через функцию free_qstr.
\param s исходная строка, копию которой нужно создать
\return новую строку, содержащую копию s. В случае ошибки возвращается пустая строка*/
extern struct qstr alloc_qstr_from_qstr(const struct qstr s);

/*! Удаляет ранее созданную строку (через функции alloc_qstr_from...) и
очищает поля структуры. Удаление пустой строки также безопасно.
\param str строка для удаления */
extern void free_qstr(struct qstr* str);

/*! Сравнивает две строки на равенство. Если строки равны, возвращается 0
\param n1, n2 строки для сравнения
\return результат сравнения двух строк. Если строки равны, то возвращается 0.
Если строки не равны, то возвращается не-0. */
extern int compare_qstr(const struct qstr n1, const struct qstr n2);

/*! Возвращает пустую строку */
extern struct qstr get_null_qstr(void);

#endif // COMMON_H
