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

#ifndef TAG_STORAGE_H
#define TAG_STORAGE_H

#include <linux/dcache.h>
#include <linux/kernel.h>

#include "tag_tag_mask.h"

#define kFSSpecialNameStartIno  0x00000010
#define kFSSpecialNameFinishIno 0x000000ff
#define kFSRealFilesStartIno    0x00000100
#define kFSRealFilesFinishIno   0x0fffffff
#define kFSDirectoriesStartIno  0x10000000
#define kFSDirectoriesFinishIno 0xffffffff


enum FSSpecialName {
  kFSSpecialNameUndefined = 0,
  kFSSpecialNameOnlyFiles = 1,
  kFSSpecialNameFilesWOTags = 2,
  kFSSpecialNameTags = 3,
  kFSSpecialNameOnlyTags = 4,
  kFSSpecialNameControl = 5
};

extern const size_t kNotFoundIno;

typedef void* Storage;

/*! Инициализация хранилища файловой системы
\param stor указатель на хранилище, который будет инициализирован новым хранилищем.
Для освобождения ресурсов нужно вызвать tagfs_release_storage.
\param file_storage имя файла, который содержит данные файловой системы
\return признак успешной инициализации (0), или отрицательный код ошибки */
int tagfs_init_storage(Storage* stor, const char* file_storage);

/*! Освобождение хранилища, закрытие ресурсов.
\param stor указатель на закрываемое хранилище. */
void tagfs_release_storage(Storage* stor);

/*! Сохранить/Сериализовать хранилище. Ошибок не возвращает */
void tagfs_sync_storage(Storage stor);


/*! Возвращает название тэга по индексу/tagino. Если индекс невалидный или тэг удалён, то возвращается строка пустой длины
\param index индекс тэга, по которому запрашивается информация
\return строка с названием тэга. Строка выделяется на памяти, необходимо её удалить */
// TODO переименовать. Убрать индекс, сделать ino
struct qstr tagfs_get_tag_name_by_index(Storage stor, size_t index);

/* ??? */
size_t tagfs_get_tagino_by_name(Storage stor, const struct qstr name);


/*! Вычитывает информацию по активному тэгу с порядковым номером index (с нуля).
При подсчёте неактивные/удалённые тэги не учитываются. Если тэг не найден,
то возвращается пустая строка и ino тэга, равный kNotFoundIno (-1).
\param index - порядковый номер тэга (с нуля)
\param exclude_mask - макса битов, которые нужно при поиске исключить
\param taginfo - возвращает ino этого тэга. Параметр может быть NULL
\return строка с именем тэга. Строку необходимо потом удалить */
struct qstr tagfs_get_nth_tag(Storage stor, size_t index,
    const struct TagMask exclude_mask, size_t* tag);


/*! Возвращает название следующего (после tagino) тэга. Индекс нового тэга
перезаписывает в tagino. Если тэгов больше нет, то возвращается пустая строка
и индекс kNotFoundIno
\param tagino индекс тэга, ПОСЛЕ которого искать следующий (size_t(-1) для поиска сначала)
\return строка с именем тэга. Если тэгов нет, то возвращается пустая строка.
Строка выделяется на памяти, её необходимо после удалить. */
struct qstr tagfs_get_next_tag(Storage stor, const struct TagMask exclude_mask,
    size_t* tagino);


/*! Возвращает имя файла по его индексу/ino. Если индекс невалидный или файл
удалён, то возвращается строка пустой длины. Строка выделяется в памяти и получатель строки должен сам её удалить.
\param index индекс файла, по которому запрашивается информация
\return строка с названием файла. Строка может быть пустой (NULL, нулевой длины) */
struct qstr tagfs_get_fname_by_ino(Storage stor, size_t ino,
    struct TagMask* mask);


/*! Возвращает специальное (зарезервированное) имя согласно входному параметру.
Используется для назначения пользовательских названий. Строку необходимо потом
удалить.
\param name тип специального имени
\return строка, содержащая специальное имя. Если такого имени нет, то
возвращается строка с NULL указателем и нулевой длиной. */
struct qstr tagfs_get_special_name(Storage stor, enum FSSpecialName name);

/*! Возвращает тип (значение перечисления) специального имени
\param name строковое имя, тип которого нужно возвратить
\return тип, соответствующий переданному имени. Если имя не найдено, то
возвращается kFSSpecialNameUndefined */
enum FSSpecialName tagfs_get_special_type(Storage stor, const struct qstr name);

// TODO Актуализировать описание
/*! Ищет файл, который соответствует маске mask и имеет порядковый индекс index.
Номера начинаются с нуля, номера уникальны, следующий поиск можно начинать с ранее найденного номера + 1.
\param on_mask маска битов, которые установлены у файла
\param off_mask маска битов, которые сброшены у файла
\param index порядковый индекс файла (отсчёт с нуля)
\param found_ino найденный номер ino. Если  файл не найден, то возвращается
значение kNotFoundIno. Параметр не может быть NULL
\return найденное имя файла. Если файл не найден, то возвращается пустая
строка. Строку потом необходимо явно удалить (пустую удалять необязательно) */
// TODO Переделать на использование, аналогично nth_tag
struct qstr tagfs_get_nth_file(Storage stor, const struct TagMask on_mask,
    const struct TagMask off_mask, size_t index, size_t* found_ino);

/*! Находит файл (следующий), подходящий по маскам on_mask/off_mask и стоящий
следующим за номер ino
\param on_mask маска битов, которые установлены у файла
\param off_mask маска битов, которые сброшены у файла
\param ino на вход - номер файла, после которого искать следующий файл. На выход - номер найденного файла или kNotFoundIno
\return найденное имя файла. Если файл не найден, то возвращается пустая
строка. Строку потом необходимо явно удалить (пустую удалять необязательно) */
struct qstr tagfs_get_next_file(Storage stor, const struct TagMask on_mask,
    const struct TagMask off_mask, size_t* ino);

/*! Находит номер файла с именем ino
\param name имя файла, номер которого нужно получить
???
\return номер файла или kNotFoundIno (если файл не найден) */
size_t tagfs_get_fileino_by_name(Storage stor, const struct qstr name,
    struct TagMask* mask);


/*! Получить результирующую ссылку на файл.
\param ino номер файла в файловой системе
\return Строка с целевой ссылкой. Строка выделяется в памяти и её нужно удалить */
struct qstr tagfs_get_file_link(Storage stor, size_t ino);


/*! Обновляем маску на существующий файл. Так как размер маски задан на этапе
формирования файловой системы и фиксирован, то размер файловой записи не меняется
\param fileino номер файла
\param mask новая маска
\return код ошибки */
int tagfs_set_file_mask(Storage stor, size_t fileino,
    const struct TagMask mask);


/*! Создадим новый файл в хранилище. И возвратим номер/ino файла.
\param target_name строка с ссылкой-путём на целевой файл. Не может быть NULL
\param link_name строка с именем символьной ссылки
\return номер созданного файла. В случае ошибки возвращается kNotFoundIno */
size_t tagfs_add_new_file(Storage stor, const char* target_name,
    const struct qstr link_name);


/*! Удалить файл из хранилища ??? */
int tagfs_del_file(Storage stor, const struct qstr file);


/*! Добавить новый тэг.
\param tag_name имя тэга
\param tagino указатель на получаемый номер (ino) тэга. Может быть NULL
\return 0 - всё хорошо, или отрицательный код ошибки */
int tagfs_add_new_tag(Storage stor, const struct qstr tag_name, size_t* tagino);


/* ??? */
size_t tagfs_get_maximum_tags_amount(Storage stor);


/* ??? */
size_t tagfs_get_active_tags_amount(Storage stor);


/*! Возвращает строку с no-префиксом. Строка константная, удалять не требуется
\return строка с префиксом. Не может быть пустой */
const struct qstr tagfs_get_no_prefix(Storage stor);

/*! Удалим тэг из файловой системы
\param tag имя тэга. Не может быть пустой строкой
\return отрицательный код ошибки. 0 - нет ошибок */
int tagfs_del_tag(Storage stor, const struct qstr tag);


#endif // TAG_STORAGE_H
