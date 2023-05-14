#ifndef DENTRY_INFO_H
#define DENTRY_INFO_H

#include "tag_tag_mask.h"

struct DentryInfo {
  struct TagMask mask; //!< Маска тэговых битов для директории
};


#endif // DENTRY_INFO_H
