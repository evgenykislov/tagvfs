#ifndef TAG_OPTIONS_H
#define TAG_OPTIONS_H

#include <linux/types.h>


struct MountOptions {
  bool DirectMode; //! Флаг использования режима прямого доступа к целевым файлам
};


/*! Очистить опции
\param opt указатель на структуру с опциями */
void ClearMountOptions(struct MountOptions* opt);


#endif
