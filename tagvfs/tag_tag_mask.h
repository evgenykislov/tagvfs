#ifndef TAG_TAG_MASK_H
#define TAG_TAG_MASK_H

#include <linux/kernel.h>


/*! Структура, описывающая набор тэгов */
struct TagMask {
  void* data;
  size_t bit_len;
  size_t byte_len;
};

/*! Выдать байтовый размер маски в зависимости от битового размера
\param mask_len - количество бит в маске
\return байтовый размер маски */
size_t tagmask_get_byte_len(size_t mask_len);

/*! Инициализировать маску пустыми флагами
\param mask_len - количество бит в маске
\return структура, описывающая маске. В случае ошибки возвразается пустая структура. Структуру нужно потом удалить через tagmask_release */
struct TagMask tagmask_init_zero(size_t mask_len);

/*! Инициализировать маску одним битом тэга
\param mask_len - см. tagmask_init_zero
\param tag выставляемый бит тэга. Нумерация с нуля.
\return см. tagmask_init_zero*/
struct TagMask tagmask_init_by_tag(size_t mask_len, size_t tag);

/*! ??? */
struct TagMask tagmask_empty(void);


/*! Удалить маску, освободить ресурсы
\param mask - удаляемая маска */
void tagmask_release(struct TagMask* mask);

/*! Проверить маску, что она пустая. Пустую маску можно не удалять -
там нет выделенных ресурсов */
bool tagmask_is_empty(const struct TagMask mask);

/*! Получить состояние бита тэга в маске.
??? */
bool tagmask_check_tag(const struct TagMask mask, size_t tag);

/* ???

Ошибки не возвращаются. Еслои бит вне границ, тогда маска не меняется
*/
void tagmask_set_tag(struct TagMask mask, size_t tag, bool state);


/* ??? */
void tagmask_fill_from_buffer(struct TagMask mask, void* buf, size_t buf_size);


/*! Логические операции над масками ??? */
void tagmask_or_mask(struct TagMask result, struct TagMask arg);

/*! ??? */
void tagmask_exclude_mask(struct TagMask result, struct TagMask arg);


/* ??? */
bool tagmask_check_filter(const struct TagMask item, const struct TagMask on_mask,
    const struct TagMask off_mask);

/* ??? */
void tagmask_printk(const struct TagMask mask);

#endif // TAG_TAG_MASK_H
