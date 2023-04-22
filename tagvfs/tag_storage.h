#ifndef TAG_STORAGE_H
#define TAG_STORAGE_H

#include <linux/dcache.h>
#include <linux/kernel.h>

enum FSSpecialName {
  kFSSpecialNameUndefined = 0,
  kFSSpecialNameAllFiles = 1,
  kFSSpecialNameFilesWOTags = 2,
  kFSSpecialNameTags = 3,
  kFSSpecialNameControl = 4
};



/*! Инициализация хранилища
\return признак успешной инициализации (0), или отрицательный код ошибки */
int tagfs_init_storage(void);

/*! Сохранить/Сериализовать хранилище. Ошибок не возвращает */
void tagfs_sync_storage(void);

/*! Возвращает общее количество тэгов. Среди тегов могут быть невалидные.
 *  Индексы всех тэгов входят в диапазон [0; amount).
\return Общее количество тэгов, среди которых могут быть пустые/невалидные */
ssize_t tagfs_get_tags_amount(void);

/*! Возвращает название тэга по индексу. Если индекс невалидный или тэг удалён,
то возвращается строка пустой длины
\param index индекс тэга, по которому запрашивается информация
\return строка с названием тэга. Строка может быть пустой. Данные для строки
валидны всегда (до завершения работы) */
struct qstr tagfs_get_tag_by_index(ssize_t index);

/*! Возвращает общее количество файлов под управлением.
Индексы всех файлов входят в диапазон [0; amount).
\return Общее количество файлов, среди которых могут быть пустые */
ssize_t tagfs_get_files_amount(void);

/*! Возвращает имя файла по его индексу. Если индекс невалидный или файл удалён,
то возвращается строка пустой длины
\param index индекс файла, по которому запрашивается информация
\return строка с названием файла. Строка может быть NULL. Данные для строки
валидны всегда (до завершения работы) */
const struct qstr* tagfs_get_fname_by_index(ssize_t index);

/*! Возвращает специальное (зарезервированное) имя согласно входному параметру.
 *  Используется для назначения пользовательских названий
 *  \param name тип специального имени
 *  \return строка, содержащая специальное имя. Если такого имени нет, то
 *  возвращается строка с NULL указателем и нулевой длиной. */
struct qstr tagfs_get_special_name(enum FSSpecialName name);

/*! Возвращает тип (значение перечисления) специального имени
\param name строковое имя, тип которого нужно возвратить
\return тип, соответствующий переданному имени. Если имя не найдено, то
возвращается kFSSpecialNameUndefined */
enum FSSpecialName tagfs_get_special_type(struct qstr name);

#endif // TAG_STORAGE_H
