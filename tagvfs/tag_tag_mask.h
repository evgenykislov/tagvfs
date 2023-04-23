#ifndef TAG_TAG_MASK_H
#define TAG_TAG_MASK_H

struct TagMask {
  void* data;
  size_t len;
};

int tagmask_init(struct TagMask* mask, size_t mask_len);
void tagmask_release(struct TagMask* mask);


int tagmask_suitable(const struct TagMask* item, const struct TagMask* actual_value,
    const struct TagMask* actual_mask);


#endif // TAG_TAG_MASK_H
