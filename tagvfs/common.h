// This file is part of tagvfs
// Copyright (C) 2023 Evgeny Kislov
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

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

/* ??? */
struct qstr alloc_qstr_from_2str(const char* str1, size_t len1, const char* str2, size_t len2);


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


/*! Отрезает от начала строки заголовок (header), если он есть, и возвращает
обрезанную строку. Если заголовка не найдено, то возваращется пустая строка.
Прим.: если исходная строка состоит только из заголовка, то тоже возвращается
пустая строка. В случае ошибок - возвращается пустая строка. Возвращённую строку
нужно удалить (если она не пустая)
\param source исходная строка
\param header заголовок, который ищется в начале исходной строки
\return обрезанную строку (если заголовок был) или пустую строку (если заголовка не было) */
struct qstr qstr_trim_header_if_exist(const struct qstr source, const struct qstr header);


/* ???? */
struct qstr qstr_add_header(const struct qstr source, const struct qstr header);

#endif // COMMON_H
