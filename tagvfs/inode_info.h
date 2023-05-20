#ifndef INODE_INFO_H
#define INODE_INFO_H

#include "tag_tag_mask.h"

struct InodeInfo {
  size_t tag_ino; //!< Номер (ino) тэга для директории (нужен для случаев удаления)
  struct TagMask on_mask; //!< Маска включённых тэговых битов для директории
  struct TagMask off_mask; //!< Маска исключённых тэговых битов для директории
};


#endif // INODE_INFO_H
