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


size_t tagmask_get_byte_len(size_t mask_len) {
  return (mask_len + kMaskSizeAlignment * 8 - 1) / (kMaskSizeAlignment * 8) *
      kMaskSizeAlignment;
}



struct TagMask tagmask_init_zero(size_t mask_len) {
  struct TagMask res;

  if (mask_len == 0) { return tagmask_empty(); }
  res.bit_len = mask_len;
  res.byte_len = (mask_len + kMaskSizeAlignment * 8 - 1) /
      (kMaskSizeAlignment * 8) * kMaskSizeAlignment;
  res.data = kzalloc(res.byte_len, GFP_KERNEL);
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
  WARN_ON(mask.data && !(mask.byte_len && mask.bit_len));
  return mask.data == NULL;
}

bool tagmask_check_tag(const struct TagMask mask, size_t tag) {
  size_t pos;
  u8 m;

  if (tag >= mask.bit_len) { return false; }
  pos = GetTagPosition(tag, &m);
  return (((u8*)mask.data)[pos] & m) != 0;
}

void tagmask_set_tag(struct TagMask mask, size_t tag, bool state) {
  size_t pos;
  u8 m;

  if (tag >= mask.bit_len) { return; }
  pos = GetTagPosition(tag, &m);
  if (state) {
    ((u8*)mask.data)[pos] |= m;
  } else {
    ((u8*)mask.data)[pos] &= ~m;
  }
}


void tagmask_fill_from_buffer(struct TagMask mask, void* buf, size_t buf_size) {
  size_t l = mask.byte_len;
  if (l > buf_size) {
    memcpy(mask.data, buf, buf_size);
    memset(mask.data + buf_size, 0, l - buf_size);
  } else {
    memcpy(mask.data, buf, l);
  }
}


/*! Логические операции над масками ??? */
void tagmask_or_mask(struct TagMask result, const struct TagMask arg) {
  size_t ml = result.byte_len;
  size_t i;

  if (arg.byte_len < ml) { ml = arg.byte_len; }
  for (i = 0; i < ml; ++i) {
    ((u8*)result.data)[i] |= ((u8*)arg.data)[i];
  }
}


void tagmask_exclude_mask(struct TagMask result, struct TagMask arg) {
  size_t ml = result.byte_len;
  size_t i;

  if (arg.byte_len < ml) { ml = arg.byte_len; }
  for (i = 0; i < ml; ++i) {
    ((u8*)result.data)[i] &= ~(((u8*)arg.data)[i]);
  }
}


bool tagmask_check_filter(const struct TagMask item, const struct TagMask on_mask,
    const struct TagMask off_mask) {
  size_t i;

  if (on_mask.byte_len != off_mask.byte_len) { return false; }
  if (item.byte_len != on_mask.byte_len) { return false; }
  if (!item.data || !on_mask.data || !off_mask.data) { return false; }

  for (i = 0; i < item.byte_len; ++i) { // TODO IMPROVE PERFORMANCE (x*8 byte)
    const u8 vi = ((const u8*)item.data)[i];
    const u8 von = ((const u8*)on_mask.data)[i];
    const u8 voff = ((const u8*)off_mask.data)[i];

    if ((vi & von) != von) { return false; }
    if ((~vi & voff) != voff) { return false; }
  }
  return true;
}


void tagmask_printk(const struct TagMask mask) {
  size_t i;

  pr_info("Mask: bit size: %zu (%zu bytes) - ", mask.bit_len, mask.byte_len);
  for (i = 0; i < mask.byte_len / 4; ++i) {
    pr_cont("%08x ", ((u32*)mask.data)[i]);
  }
  pr_cont("\n");
}
