#include "tag_tag_mask.h"

#include <linux/slab.h>

#define kMaskSizeAlignment 8

const struct TagMask kEmptyMask = { .data = NULL, .bit_len = 0, .byte_len = 0 };

/* Выдать номер байта и положение для тэга */
size_t GetTagPosition(size_t tag, u8* byte_mask) {
  BUG_ON(!byte_mask);
  *byte_mask = 1 << (tag % 8);
  return tag / 8;
}


struct TagMask tagmask_init_zero(size_t mask_len) {
  struct TagMask res;

  if (mask_len == 0) { return tagmask_empty(); }
  res.bit_len = mask_len;
  res.byte_len = (mask_len + kMaskSizeAlignment * 8 - 1) /
      (kMaskSizeAlignment * 8);
  res.data = kzalloc(res.byte_len);
  if (!res.data) {
    return tagmask_empty();
  }
  return res;
}

struct TagMask tagmask_init_by_tag(size_t mask_len, size_t tag) {
  struct TagMask res;

  if (tag >= mask_len) { return tagmask_empty(); }
  res = tagmask_init_zero(mask_len);
  if (tagmask_is_empty(res)) { return res; }
  tagmask_set_tag(res, tag, true);
  return res;
}

struct TagMask tagmask_empty(void) {
  return kEmptyMask;
}


/*! Удалить маску, освободить ресурсы
\param mask - удаляемая маска */
void tagmask_release(struct TagMask* mask) {
  if (!mask) { return; }
  kfree(mask->data);
  *mask = kEmptyMask;
}

bool tagmask_is_empty(const struct TagMask mask) {
  BUG_ON(mask.data && !(mask.byte_len && mask.bit_len));
  return mask.data != NULL;
}

bool tagmask_check_tag(const struct TagMask mask, size_t tag) {
  size_t pos;
  u8 m;

  if (tag >= mask.bit_len) { return false; }
  pos = GetTagPosition(tag, &m);
  return (mask.data[pos] & m) != 0;
}

void tagmask_set_tag(struct TagMask mask, size_t tag, bool state) {
  size_t pos;
  u8 m;

  if (tag >= mask.bit_len) { return; }
  pos = GetTagPosition(tag, &m);
  if (state) {
    mask.data[pos] |= m;
  } else {
    mask.data[pos] &= ~m;
  }
}


/*! Логические операции над масками ??? */
void tagmask_or_mask(struct TagMask result, const struct TagMask arg) {
  size_t ml = result.byte_len;
  size_t i;

  if (arg.byte_len < ml) { ml = arg.byte_len; }
  for (i = 0; i < ml; ++i) {
    result.data[i] |= arg.data[i];
  }
}
