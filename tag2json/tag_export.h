#ifndef TAG_EXPORT_H
#define TAG_EXPORT_H

#include <string>

/*! Экспорт информации о тэгах и файлах во внешний json файл
\param src имя файла с исходной информацией о тэгах
\param dst имя файла, в который будет записана информация о тэгах в json формате
\param rewrite признак принудительной перезаписи файла
\return код ошибки (код завершения процесса). 0 - нет ошибок */
int TagExport(const std::string& src, const std::string& dst, bool rewrite);

#endif // TAG_EXPORT_H
