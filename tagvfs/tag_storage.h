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
\param stor указатель на закрываемое хранилище. */
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

/*! Возвращает общее количество файлов под управлением. Все файлы валидные,
удалённые не учитываются.
\return Общее количество файлов. В случае ошибки возвращается 0 */
ssize_t tagfs_get_files_amount(Storage stor);

/*! Возвращает имя файла по его индексу/ino. Если индекс невалидный или файл
удалён, то возвращается строка пустой длины. Строка выделяется в памяти и получатель строки должен сам её удалить.
\param index индекс файла, по которому запрашивается информация
\return строка с названием файла. Строка может быть пустой (NULL, нулевой длины) */
struct qstr tagfs_get_fname_by_ino(Storage stor, size_t ino);


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

/*! Ищет первое вхождение файла, который имеет номер равный или более start_ino и
соответствует маске mask. Номера начинаются с нуля, номера уникальны, следующий поиск можно начинать с ранее найденного номера + 1.
\param start_ino начальный номер, с которого начинать поиск
\param mask требуемая маска для файла
\param found_ino найденный номер. Может быть равен start_ino или быть больше.
Если  файл не найден, то возвращается значение kNotFoundIno. Параметр не может
быть NULL
\param name найденное имя файла. Если файл не найден, то возвращается пустая
строка. Строку потом необходимо явно удалить. Если
значение имени не требуется, то можно параметр передавать как NULL */
void tagfs_get_first_name(Storage stor, size_t start_ino,
    const struct TagMask* mask, size_t* found_ino, struct qstr* name);

/*! Находит номер файла с именем ino
\param name имя файла, номер которого нужно получить
\return номер файла или kNotFoundIno (если файл не найден) */
size_t tagfs_get_ino_of_name(Storage stor, const struct qstr name);


/*! Получить результирующую ссылку на файл.
\param ino номер файла в файловой системе
\return Строка с целевой ссылкой. Строка выделяется впамяти и её нужно удалить */
struct qstr tagfs_get_file_link(Storage stor, size_t ino);


/*! Создадим новый файл в хранилище. И возвратим номер/ino файла.
\param target_name строка с ссылкой-путём на целевой файл. Не может быть NULL
\param link_name строка с именем символьной ссылки
\return номер созданного файла. В случае ошибки возвращается kNotFoundIno */
size_t tagfs_add_new_file(Storage stor, const char* target_name,
    const struct qstr link_name);

#endif // TAG_STORAGE_H
