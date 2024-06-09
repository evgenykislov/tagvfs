#ifndef TAG_FILE_H
#define TAG_FILE_H

#include <functional>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>


class TagFileReader {
 public:
  /*! Загрузим информацию из файлового потока. В случае ошибок информация может
  быть загружена частично (и вернуть false).
  \param file файловый поток
  \return признак успешности загрузки данных */
  virtual bool Load(std::ifstream& file) = 0;

  virtual bool GetTagInfo(uint16_t tag_index, std::string& name) = 0;
  virtual void EnumTags(std::function<void(uint16_t, const std::string&)> func) = 0;
  virtual void EnumFiles(std::function<void(const std::string&, const std::string&,
      const uint16_t*, size_t)> func) = 0;
  virtual uint16_t GetTagRecordSize() = 0;
  virtual uint16_t GetTagMaxAmount() = 0;
  virtual uint16_t GetFileBlockSize() = 0;
};

std::shared_ptr<TagFileReader> GetReader(std::ifstream& file);



#endif // TAG_FILE_H
