#include "tag_storage_cache.h"

#include <linux/slab.h>

#include "common.h"


struct CacheInternal {
  struct hlist_head* NameCache;
  struct hlist_head* InoCache;
  rwlock_t CacheLock;
  size_t CacheSize;
  unsigned int CacheBits;
  size_t OvermaxIno;
};

struct ItemInternal {
  struct hlist_node NameNode;
  struct hlist_node InoNode;
  struct CacheItem Item;
  atomic_t LinkCounter_1; //!< Счётчик ссылок
};

unsigned int hash_name(const struct qstr name) {
  return full_name_hash(NULL, name.name, name.len);
}

void unlink_item_wo_lock(struct ItemInternal* item) {
  hash_del(&item->NameNode);
  hash_del(&item->InoNode);
}


void delete_item_wo_lock(struct ItemInternal* item) {

  ??? link counter

  free_qstr(&item->Item.Name);
  if (item->Item.data_remover) {
    item->Item.data_remover(item->Item.user_data);
  }
  kfree(item);
}


int tagfs_init_cache(Cache* cache, unsigned int cache_bits, size_t overmax_ino) {
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
  ci->OvermaxIno = overmax_ino;

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


void tagfs_release_cache(Cache* cache) {
  struct CacheInternal* ci;
  int bkt;
  struct ItemInternal* item;

  if (cache == NULL || (*cache) == NULL) { return; }

  ci = (struct CacheInternal*)(*cache);

  if (write_trylock(&ci->CacheLock) == 0) {
    write_unlock(&ci->CacheLock);
  } else {
    // Ошика в логике: блокировка ещё используется
    WARN(1, "Tagvfs Logic Error: cache lock as active yet\n");
  }

  // Удалим все элементы
  for (bkt = 0, item = NULL; item == NULL && bkt < ci->CacheSize; ++bkt) {
    struct hlist_node* tmp;
    hlist_for_each_entry_safe(item, tmp, &ci->InoCache[bkt], InoNode) {
      delete_item_wo_lock(item);
    }
  }

  kfree(ci->InoCache);
  kfree(ci->NameCache);
  kfree(ci);

  *cache = NULL;
}


/*! Увеличивает счётчик ссылок */
CacheIterator item_by_name_wo_lock_1(struct CacheInternal* ci, const struct qstr name) {
  unsigned int key;
  struct ItemInternal* it;

  key = hash_name(name);
  hlist_for_each_entry(it, &ci->NameCache[hash_min(key, ci->CacheBits)], NameNode) {
    if (compare_qstr(it->Item.Name, name) != 0) { continue; }
    ++it->LinkCounter;
    return &(it->Item);
  }
  return NULL;
}


/*! Увеличивает счётчик ссылок */
CacheIterator item_by_ino_wo_lock_1(struct CacheInternal* ci, size_t ino) {
  struct ItemInternal* it;

  hlist_for_each_entry(it, &ci->InoCache[hash_min(ino, ci->CacheBits)], InoNode) {
    if (it->Item.Ino != ino) { continue; }
    ++it->LinkCounter;
    return &(it->Item);
  }
  return NULL;
}


/*! Создадим элемент и добавим его в два кэша
\return отрицательный код ошибки. Если ошибок нет - 0 */
int create_add_item_wo_lock(struct CacheInternal* ci, size_t ino,
    const struct qstr name, void* user_data, void (*data_remover)(void*)) {
  unsigned int name_key = hash_name(name);
  struct ItemInternal* item = kzalloc(sizeof(struct ItemInternal), GFP_KERNEL);
  struct qstr cpname = alloc_qstr_from_qstr(name);
  if (!item) { goto err; }

  item->Item.Ino = ino;
  item->Item.Name.name = cpname.name;
  item->Item.Name.len = cpname.len;
  cpname.name = NULL;
  cpname.len = 0;
  item->Item.user_data = user_data;
  item->Item.data_remover = data_remover;

  hlist_add_head(&item->InoNode, &ci->InoCache[hash_min(ino, ci->CacheBits)]);
  hlist_add_head(&item->NameNode, &ci->NameCache[hash_min(name_key, ci->CacheBits)]);
  item->LinkCounter = 1;
  return 0;
  // --------------
err:
  free_qstr(&cpname);
  kfree(item);
  return -ENOMEM;
}


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

int tagfs_insert_item(Cache cache, size_t ino, const struct qstr name, void* data, void (*remover)(void*)) {
  struct CacheInternal* ci;
  int res;


  if (unlikely(cache == NULL || name.name == NULL || name.len == 0)) { return -EINVAL; }

  ci = (struct CacheInternal*)(cache);
  if (unlikely(ino >= ci->OvermaxIno)) { return -ERANGE; }

  write_lock(&ci->CacheLock);
  if (item_by_ino_wo_lock(ci, ino)) {
    res = -EEXIST;
  } else {
    if (item_by_name_wo_lock(ci, name)) {
      res = -EEXIST;
    } else {
      // Добавляем
      res = create_add_item_wo_lock(ci, ino, name, data, remover);
    }
  }

  write_unlock(&ci->CacheLock);
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
  delete_item_wo_lock(item);
}


void tagfs_release_item(CacheIterator it) {
  struct ItemInternal* item;

  if (unlikely(!it)) { return; }

  item = container_of(it, struct ItemInternal, Item);
  delete_item_wo_lock(item);
}
