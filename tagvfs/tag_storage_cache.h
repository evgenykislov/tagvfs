#ifndef TAG_STORAGE_CACHE_H
#define TAG_STORAGE_CACHE_H

#include <linux/dcache.h>
#include <linux/hashtable.h>

typedef void* Cache;

struct CacheItem {
  size_t Ino;
  struct qstr Name;
  void* user_data;
  void (*data_remover)(void*);
};

typedef struct CacheItem* CacheIterator;


/*! Создать и инициализировать кэш
\param cache указатель на переменную, которая будет хранить кэш. Изначально в переменно должен быть NULL
\param cache_bits количество битов в ключе кэша
\param overmax_ino овер-максимальный номер хранения ino. Т.е. хранится будут все номера от 0 до overmax_ino-1
\return отрицательный код ошибки. Если ошибок нет - возвращается 0 */
int tagfs_init_cache(Cache* cache, unsigned int cache_bits, size_t overmax_ino);

/*! Удалить кэш, очистить все данные
\param cache указатель на переменную, в которой хранится удаляемый кэш */
void tagfs_release_cache(Cache* cache);

/*! Найти элемент в кэше по имени. Если элемент найден, то при возврате
увеличивается счётчик ссылок. В дальнейшем нужно будет вызвать tagfs_release_item.
\param cache кэш-хранилище
\param name имя для поиска
\return указатель на элемент к кэше. Если элемента нет, то NULL */
const CacheIterator tagfs_get_item_by_name(Cache cache, const struct qstr name);

/*! Найти элемент в кэше по номеру/индексу. Если элемент найден, то при возврате
увеличивается счётчик ссылок. В дальнейшем нужно будет вызвать tagfs_release_item.
\param cache кэш-хранилище
\param ino номер для поиска
\return указатель на элемент к кэше. Если элемента нет, то NULL */
const CacheIterator tagfs_get_item_by_ino(Cache cache, size_t ino);

/*! Вставить элемент в кэш. В кэше не должно быть элемента-дубликата с номером ино.
Также не должно быть дублирующего имени (прим.: пустые имена могут дублироваться)
Если будет дубликат, то функция вернёт -EEXIST.
Если номер ино превышает границу кэша, то вернётся -ERANGE
В случае ошибок элемент не добавляется; блок пользовательских данных очищается.
\param cache кэш-хранилище
\param ino номер/индекс элемента. Номер 0 - допустимое значение
\param name имя элемента. Имя не может быть пустым (иначе вернётся ошибка -EINVAL)
\param data указатель на пользовательские данные
\param remover функция удаления пользовательских данных
\return код ошибки. Если ошибок нет, то возваращается 0 */
int tagfs_insert_item(Cache cache, size_t ino, const struct qstr name, void* data,
    void (*remover)(void*));

/*! Удалить элемент из хранилища. Содержимое остаётся валидным и после удаления
из хранилище - до тех пор, пока не будут освобождены все копии
\param cache кэш-хранилище
\param item элемент для удаления */
void tagfs_delete_item(Cache cache, CacheIterator item);

/*! Освободить найденный ранее элемент */
void tagfs_release_item(CacheIterator it);


#endif // TAG_STORAGE_CACHE_H
