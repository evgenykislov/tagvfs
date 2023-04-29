#ifndef TAG_STORAGE_H
#define TAG_STORAGE_H

#include <linux/dcache.h>
#include <linux/kernel.h>

#include "tag_tag_mask.h"

#define kFSSpecialNameStartIno  0x00000010
#define kFSSpecialNameFinishIno 0x000000ff
#define kFSRealFilesStartIno    0x00000100
#define kFSRealFilesFinishIno   0xefffffff
#define kFSDirectoriesStartIno  0xf0000100
#define kFSDirectoriesFinishIno 0xffffffff


enum FSSpecialName {
  kFSSpecialNameUndefined = 0,
  kFSSpecialNameAllFiles = 1,
  kFSSpecialNameFilesWOTags = 2,
  kFSSpecialNameTags = 3,
  kFSSpecialNameControl = 4
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
\param stor указатель на закрываемое хранилище.
*/
void tagfs_release_storage(Storage* stor);

/*! Сохранить/Сериализовать хранилище. Ошибок не возвращает */
void tagfs_sync_storage(Storage stor);

/*! Возвращает общее количество тэгов. Среди тегов могут быть невалидные.
 *  Индексы всех тэгов входят в диапазон [0; amount).
\return Общее количество тэгов, среди которых могут быть пустые/невалидные */
ssize_t tagfs_get_tags_amount(Storage stor);

/*! Возвращает название тэга по индексу. Если индекс невалидный или тэг удалён,
то возвращается строка пустой длины
\param index индекс тэга, по которому запрашивается информация
\return строка с названием тэга. Строка может быть пустой. Данные для строки
валидны всегда (до завершения работы) */
struct qstr tagfs_get_tag_by_index(Storage stor, ssize_t index);

/*! Возвращает общее количество файлов под управлением.
Индексы всех файлов входят в диапазон [0; amount).
\return Общее количество файлов, среди которых могут быть пустые */
ssize_t tagfs_get_files_amount(Storage stor);

/*! Возвращает имя файла по его индексу. Если индекс невалидный или файл удалён,
то возвращается строка пустой длины
\param index индекс файла, по которому запрашивается информация
\return строка с названием файла. Строка может быть NULL и нулевой длины. Данные для строки
валидны всегда (до завершения работы) */
const struct qstr tagfs_get_fname_by_index(Storage stor, ssize_t index);

/*! Возвращает специальное (зарезервированное) имя согласно входному параметру.
 *  Используется для назначения пользовательских названий
 *  \param name тип специального имени
 *  \return строка, содержащая специальное имя. Если такого имени нет, то
 *  возвращается строка с NULL указателем и нулевой длиной. */
struct qstr tagfs_get_special_name(Storage stor, enum FSSpecialName name);

/*! Возвращает тип (значение перечисления) специального имени
\param name строковое имя, тип которого нужно возвратить
\return тип, соответствующий переданному имени. Если имя не найдено, то
возвращается kFSSpecialNameUndefined */
enum FSSpecialName tagfs_get_special_type(Storage stor, struct qstr name);

/*! Ищет первое вхождение файла, который имеет номер равный или более start_ino и
соответствует маске mask. Номера начинаются с нуля, номера уникальны, следующий поиск можно начинать с ранее найденного номера + 1.
\param start_ino начальный номер, с которого начинать поиск
\param mask требуемая маска для файла
\param found_ino найденный номер. Может быть равен start_ino или быть больше.
Если  файл не найден, то возвращается значение kNotFoundIno.
\param name найденное имя файла. Имя существует на всё время работы программы. */
void tagfs_get_first_name(Storage stor, size_t start_ino,
    const struct TagMask* mask, size_t* found_ino, struct qstr* name);

/* Находит номер файла с именем ino
\param name имя файла, номер которого нужно получить
\return номер файла или kNotFoundIno (если файл не найжен) */
size_t tagfs_get_ino_of_name(Storage stor, const struct qstr name);

/*! Получить размер файла в байтах
\param ino номер файла в файловой системе
\return размер файла в байтах (и может быть нулевым). В случае ошибки возвращается 0. */
size_t tagfs_get_file_size(Storage stor, size_t ino);

/*! Прочитать данные из файла. Данные читаются бинарные (сырые). Все длины, позиции
измеряются в байтах.
\param ino номер файла в фасйловой системе
\param pos стартовая позиция для чтения
\param len длина блока для чтения. Должна быть больше 0
\param buffer буфер под данные
\return размер прочитанных данных. Если всё хорошо, то соответствует len */
size_t tagfs_get_file_data(Storage stor, size_t ino, size_t pos, size_t len,
    void* buffer);


/*! Создадим новый файл
\param название файла
\return номер созданного файла. В случае ошибки возвращается kNotFoundIno */
size_t tagfs_add_new_file(Storage stor, const char* target_name,
    const struct qstr link_name);

#endif // TAG_STORAGE_H
