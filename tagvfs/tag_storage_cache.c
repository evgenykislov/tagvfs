#include "tag_storage_cache.h"

#include <linux/slab.h>

#include "common.h"


struct CacheInternal {
  struct hlist_head* NameCache;
  struct hlist_head* InoCache;
  rwlock_t CacheLock;
  size_t CacheSize;
  unsigned int CacheBits;
};

struct ItemInternal {
  /*! Счётчик ссылок. При создании структуры и добавлении в хэш номеров счётчик
  сразу ставится в 1. При удалении из хэша номеров счётсик уменьшается на 1 */
  atomic_t LinkCounter;
  struct hlist_node InoNode;
  struct hlist_node NameNode;
  bool InoHashed; //!< Признак, что структура внесена в хэш номеров
  bool NameHashed; //!< Признак, что структура внесена в хэш имён

  struct CacheItem Item;
};

/*! Функция, вычисляющая хэш имени
\param name строка (имя), по которой нужно сосчитать хэш
\return значение хэша */
unsigned int hash_name(const struct qstr name) {
  return full_name_hash(NULL, name.name, name.len);
}


/*! "Заявим" удаление элемента. Если элементом ещё пользуются, то фактического
удаления не будет
\param item элемент для удаления */
void delete_item_wo_lock(struct ItemInternal* item) {
  if (!atomic_dec_and_test(&item->LinkCounter)) { return; }

  // So, counter is zero. Make real deletion
  free_qstr(&item->Item.Name);
  if (item->Item.data_remover) {
    item->Item.data_remover(item->Item.user_data);
  }
  kfree(item);
}


/*! Отвяжем элемент из обеих хэш-таблиц. Эта функция не удаляет сам элемент
\param item элемент для отвязки */
void unlink_item_wo_lock(struct ItemInternal* item) {
  bool need_dec = false;

  if (item->InoHashed) {
    hash_del(&item->InoNode);
    item->InoHashed = false;
    need_dec = true;
  }
  if (item->NameHashed) {
    hash_del(&item->NameNode);
    item->NameHashed = false;
  }

  if (need_dec) {
    delete_item_wo_lock(item);
  }
}


// Описание в хедере
int tagfs_init_cache(Cache* cache, unsigned int cache_bits) {
  struct CacheInternal* ci;
  size_t cache_size = 1 << cache_bits;
  size_t i;

  if (cache == NULL || (*cache) != NULL) { return -EINVAL; }

  ci = kzalloc(sizeof(struct CacheInternal), GFP_KERNEL);

  if (!ci) { goto err_cache; }
  ci->NameCache = kzalloc(sizeof(struct hlist_head) * cache_size, GFP_KERNEL);
  if (!ci->NameCache) { goto err_mem; }
  ci->InoCache = kzalloc(sizeof(struct hlist_head) * cache_size, GFP_KERNEL);
  if (!ci->InoCache) { goto err_mem; }

  // Инициализация кэшей
  for (i = 0; i < cache_size; ++i) {
    INIT_HLIST_HEAD(&ci->NameCache[i]);
    INIT_HLIST_HEAD(&ci->InoCache[i]);
  }

  rwlock_init(&ci->CacheLock);
  ci->CacheSize = cache_size;
  ci->CacheBits = cache_bits;

  *cache = ci;
  return 0;
  // ----------
err_mem:
  kfree(ci->InoCache);
  kfree(ci->NameCache);
  kfree(ci);
err_cache:
  return -ENOMEM;
}


// Описание в хедере
void tagfs_release_cache(Cache* cache) {
  struct CacheInternal* ci;
  int bkt;
  struct ItemInternal* item;

  if (cache == NULL || (*cache) == NULL) { return; }

  ci = (struct CacheInternal*)(*cache);

  if (write_trylock(&ci->CacheLock) == 0) {
    // Ошибка в логике: блокировка ещё используется. Оставляем все ресурсы в утечку памяти.
    WARN(1, "Tagvfs Logic Error: cache lock as active yet\n");
    return;
  }

  // Кэшем никто не пользуется. Вот сейчас всё и подчистим
  // Удалим все элементы
  for (bkt = 0, item = NULL; item == NULL && bkt < ci->CacheSize; ++bkt) {
    struct hlist_node* tmp;
    hlist_for_each_entry_safe(item, tmp, &ci->InoCache[bkt], InoNode) {
      if (item->InoHashed) {
        delete_item_wo_lock(item);
      }
      item->InoHashed = false;
      item->NameHashed = false;
    }
  }

  kfree(ci->InoCache);
  kfree(ci->NameCache);

  write_unlock(&ci->CacheLock);

  kfree(ci);
  *cache = NULL;
}


/*! Ищем элемент кэша по имени и возвращаем что нашли. У найденного элемента
увеличиваем счётчик ссылок использования.
\param name имя для поиска
\return найденный элемент. Если элемент не найден, то возвращается NULL */
CacheIterator item_by_name_wo_lock(struct CacheInternal* ci, const struct qstr name) {
  unsigned int key;
  struct ItemInternal* it;

  key = hash_name(name);
  hlist_for_each_entry(it, &ci->NameCache[hash_min(key, ci->CacheBits)], NameNode) {
    if (compare_qstr(it->Item.Name, name) != 0) { continue; }
    atomic_inc(&it->LinkCounter);
    return &(it->Item);
  }
  return NULL;
}


/*! Ищем элемент кэша по номеру и возвращаем что нашли. У найденного элемента
увеличиваем счётчик ссылок использования.
\param ino номер для поиска
\return найденный элемент. Если элемент не найден, то возвращается NULL */
CacheIterator item_by_ino_wo_lock(struct CacheInternal* ci, size_t ino) {
  struct ItemInternal* it;

  hlist_for_each_entry(it, &ci->InoCache[hash_min(ino, ci->CacheBits)], InoNode) {
    if (it->Item.Ino != ino) { continue; }
    atomic_inc(&it->LinkCounter);
    return &(it->Item);
  }
  return NULL;
}


/*! Создадим элемент и добавим его кэш номеров. Если имя не пустое, то добавляем
в кэш имён. В случае ошибок пользовательские данные НЕ удаляются (т.к. это
внутренняя функция и обработка всех ошибок производится в базовых функциях).
\param ino номер элемента
\param name имя элемента. Может быть пустым
\param user_data пользовательские данные. Могут быть NULL
\param data_remover функция удаления пользовательских данных. Может быть NULL
\return отрицательный код ошибки. Если ошибок нет - 0 */
int create_add_item_wo_lock(struct CacheInternal* ci, size_t ino,
    const struct qstr name, void* user_data, void (*data_remover)(void*)) {
  unsigned int name_key = hash_name(name);
  struct ItemInternal* item = kzalloc(sizeof(struct ItemInternal), GFP_KERNEL);
  if (!item) {
    return -ENOMEM;
  }

  item->Item.Ino = ino;
  item->Item.Name = get_null_qstr();
  item->Item.user_data = user_data;
  item->Item.data_remover = data_remover;

  hlist_add_head(&item->InoNode, &ci->InoCache[hash_min(ino, ci->CacheBits)]);
  item->InoHashed = true;
  if (name.name && name.len) {
    item->Item.Name = alloc_qstr_from_qstr(name);
    hlist_add_head(&item->NameNode, &ci->NameCache[hash_min(name_key, ci->CacheBits)]);
  }
  item->NameHashed = true;
  atomic_set(&item->LinkCounter, 1);
  return 0;
}


// Описание в хедере
const CacheIterator tagfs_get_item_by_name(Cache cache, const struct qstr name) {
  struct CacheInternal* ci;
  CacheIterator it;

  if (unlikely(cache == NULL || name.len == 0 || name.name == NULL)) { return NULL; }

  ci = (struct CacheInternal*)(cache);
  read_lock(&ci->CacheLock);
  it = item_by_name_wo_lock(ci, name);
  read_unlock(&ci->CacheLock);
  return it;
}


// Описание в хедере
const CacheIterator tagfs_get_item_by_ino(Cache cache, size_t ino) {
  struct CacheInternal* ci;
  CacheIterator it;

  if (unlikely(cache == NULL)) { return NULL; }

  ci = (struct CacheInternal*)(cache);
  read_lock(&ci->CacheLock);
  it = item_by_ino_wo_lock(ci, ino);
  read_unlock(&ci->CacheLock);
  return it;
}


// Описание в хедере
int tagfs_insert_item(Cache cache, size_t ino, const struct qstr name, void* data, void (*remover)(void*)) {
  struct CacheInternal* ci;
  CacheIterator it;
  int res = 0;

  if (unlikely(cache == NULL || name.name == NULL || name.len == 0)) { return -EINVAL; }

  ci = (struct CacheInternal*)(cache);

  write_lock(&ci->CacheLock);

  it = item_by_ino_wo_lock(ci, ino);
  if (it) {
    tagfs_release_item(it);
    res = -EEXIST;
  } else {
    it = item_by_name_wo_lock(ci, name);
    if (it) {
      tagfs_release_item(it);
      res = -EEXIST;
    } else {
      // Добавляем
      res = create_add_item_wo_lock(ci, ino, name, data, remover);
    }
  }

  write_unlock(&ci->CacheLock);

  if (res && data && remover) {
    remover(data);
  }
  return res;
}

void tagfs_delete_item(Cache cache, CacheIterator it)
{
  struct CacheInternal* ci;
  struct ItemInternal* item;

  if (unlikely(!cache || !it)) { return; }

  ci = (struct CacheInternal*)(cache);
  item = container_of(it, struct ItemInternal, Item);
  write_lock(&ci->CacheLock);
  unlink_item_wo_lock(item);
  write_unlock(&ci->CacheLock);
}


void tagfs_release_item(CacheIterator it) {
  struct ItemInternal* item;

  if (unlikely(!it)) { return; }

  item = container_of(it, struct ItemInternal, Item);
  delete_item_wo_lock(item);
}


int tagfs_hold_ino(Cache cache, size_t ino) {
  struct CacheInternal* ci;
  CacheIterator it;
  int res = 0;

  if (unlikely(cache == NULL)) { return -EINVAL; }

  ci = (struct CacheInternal*)(cache);
  write_lock(&ci->CacheLock);
  it = item_by_ino_wo_lock(ci, ino);
  if (it == NULL) {
    res = -ENOENT;
  } else {
    struct ItemInternal* item = container_of(it, struct ItemInternal, Item); //!< Элемент кэша на удаление. Он ещё может быть в использовании другими
    if (!it->Name.name && it->Name.len == 0) {
      // Элемент уже в блокировке
      res = -EINVAL;
    } else {
      // Удалим элемент с требуемым номером из кэша
      unlink_item_wo_lock(item);
      // Создадим элемент с требуемым номером
      // Такое удаление/создание требуется, т.к. элемент может быть в использовании
      res = create_add_item_wo_lock(ci, ino, get_null_qstr(), NULL, NULL);
    }
    delete_item_wo_lock(item);
  }
  write_unlock(&ci->CacheLock);

  // Пользовательские данные после ошибок create_add_item_wo_lock удалять не нужно, т.к. там всё NULL

  return res;
}
