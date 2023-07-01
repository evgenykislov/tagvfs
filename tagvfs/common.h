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

//! Позиция после точечных директорий для файловой итерации
#define kPosAfterDots (2)

// Функции-хелперы
// ---------------

/*! Функция-хелпер для получения указателя на хранилище из суперблока
\param sb суперблок. Не может быть NULL
\return указатель на хранилище */
void* super_block_storage(const struct super_block* sb);

/*! Функция-хелпер для получения указателя на хранилище из ноды файла или директории
\param nod указатель на ноду. Не может быть NULL
\return указатель на хранилище */
void* inode_storage(const struct inode* nod);

// Строковые функции
// -----------------
// Строковые фукнции создают новые строки на основе переданных данных
// Строку-результат необходимо после использования удалить функцией free_str
// В случае ошибок может возвращаться пустая строка, у которой поле name == NULL
// и len == 0. Пустую строку можно не удалять, т.к. она не содержит выделенных
// ресурсов

/*! Создаёт новую строку и заполняет её данными.
Созданную строку нужно потом явно удалить через функцию free_qstr
\param str строка, которую скопировать в результат
\param len длина строки. Может быть 0. Не может быть -1
\return строка qstr с копией сторочных данных или пустая строка в случае ошибки */
struct qstr alloc_qstr_from_str(const char* str, size_t len);

/*! Создаёт новую строку и заполняет её данными из двух исходных строк.
Созданную строку нужно потом явно удалить через функцию free_qstr
\param str1 строка, которую скопировать в результат
\param len1 длина строки. Может быть 0. Не может быть -1
\param str2 строка, которую скопировать в результат
\param len2 длина строки. Может быть 0. Не может быть -1
\return строка qstr объединением двух строк (str1 + str2) или пустая строка в случае ошибки */
struct qstr alloc_qstr_from_2str(const char* str1, size_t len1, const char* str2,
    size_t len2);

/*! Создаёт новую строку и заполняет её данными из исходного аргумента.
Созданную строку нужно потом явно удалить через функцию free_qstr
\param s исходная строка, копию которой нужно создать
\return новую строку, содержащую копию s. В случае ошибки возвращается пустая строка */
struct qstr alloc_qstr_from_qstr(const struct qstr s);

/*! Удаляет ранее созданную строку (через функции alloc_qstr_from...) и
очищает поля структуры. Допустимо удаление пустой строки - это безопасно.
\param str строка для удаления. После удаления содержимое заполняется как пустая строка */
void free_qstr(struct qstr* str);

/*! Сравнивает две строки на равенство. Если строки равны, возвращается 0
\param n1, n2 строки для сравнения
\return результат сравнения двух строк. Если строки равны, то возвращается 0.
Если строки не равны, то возвращается не-0. */
int compare_qstr(const struct qstr n1, const struct qstr n2);

/*! Возвращает пустую строку
\param структура с пустой строкой */
struct qstr get_null_qstr(void);

/*! Отрезает от начала строки заголовок (header) при наличии, и возвращает
обрезанную строку. Если заголовка не найдено, то возваращется пустая строка.
Прим.: если исходная строка состоит только из заголовка, то тоже возвращается
пустая строка. В случае ошибок - возвращается пустая строка. Возвращённую строку
нужно удалить
\param source исходная строка
\param header заголовок, который ищется в начале исходной строки
\return обрезанная строка (если заголовок был) или
пустая строка (если заголовка не было или произошла ошибка) */
struct qstr qstr_trim_header_if_exist(const struct qstr source, const struct qstr header);

/*! Добавляет к строке source заголовок header. Возвращается новая строка, которыю
нужно удалить
\param source исходная строка
\param header заголовок для добавления
\return строка с заголовком или пустая строка в случае ошибки */
struct qstr qstr_add_header(const struct qstr source, const struct qstr header);

/*! Общая функция поиска по директории */
loff_t tagfs_common_dir_llseek(struct file* f, loff_t offset, int whence);


#endif // COMMON_H
