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


/* ??? */
struct TagMask tagmask_init_by_mask(const struct TagMask mask);

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

/* ??? */
size_t tagmask_on_bits_amount(const struct TagMask mask);

/* Выставляем/Сбрасываем тэговый бит в маске. Никаких ошибок не возвращается.
Если бит вне границ маски, тогда маска не меняется
\param mask маска, в которой меняется тэговый бит
\param tag номер тэга, для которого изменяется состояние
\state требуемое состояние */
void tagmask_set_tag(struct TagMask mask, size_t tag, bool state);


/*! Заполняет маску данными из буфера. Если маска больше буфера, то остаток
маски обнуляется. Если маска меньше буфера, то заполняются только данные,
которые влезают в маску. Заполнение всегда успешно (ошибок не возвращается)
\param mask маска, которая заполняется данными из буфера. Маска должна быть
инициализирована (и желаемым размером). Исходные данные маски будут вычищены
\param buf, buf_size буфер с данными для заполнения маски */
void tagmask_fill_from_buffer(struct TagMask mask, void* buf, size_t buf_size);


/*! Логические операции над масками ??? */
void tagmask_or_mask(struct TagMask result, struct TagMask arg);

/*! ??? */
void tagmask_exclude_mask(struct TagMask result, struct TagMask arg);


/* ??? */
bool tagmask_check_filter(const struct TagMask item, const struct TagMask on_mask,
    const struct TagMask off_mask);

/*! Отладочный вывод информации о маске в kern.log
\param mask выводиимая маска */
void tagmask_printk(const struct TagMask mask);

#endif // TAG_TAG_MASK_H
