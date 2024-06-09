#ifndef TAG_FILE_43235634_H
#define TAG_FILE_43235634_H

#include <functional>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../tag_file.h"


/*! Класс с информацией о тэгах, файлах, настройках */
class TagFileReader43235634: public TagFileReader {
 public:

  struct TagInfo {
    uint16_t Index;
    uint16_t Flags;
    std::string Name;
  };

  struct FileInfo {
    std::string FileName;
    std::string TargetName;
    std::vector<uint16_t> Tags;
  };


  TagFileReader43235634();

  /*! Загрузим информацию из файлового потока. В случае ошибок информация может
  быть загружена частично (и вернуть false).
  \param file файловый поток
  \return признак успешности загрузки данных */
  bool Load(std::ifstream& file);

  bool GetTagInfo(uint16_t tag_index, std::string& name);
  void EnumTags(std::function<void(uint16_t, const std::string&)> func);
  void EnumFiles(std::function<void(const std::string&, const std::string&,
      const uint16_t*, size_t)> func);
  uint16_t GetTagRecordSize();
  uint16_t GetTagMaxAmount();
  uint16_t GetFileBlockSize();


 private:
  TagFileReader43235634(const TagFileReader43235634&) = delete;
  TagFileReader43235634(TagFileReader43235634&&) = delete;
  TagFileReader43235634& operator=(const TagFileReader43235634&) = delete;
  TagFileReader43235634& operator=(TagFileReader43235634&&) = delete;

  struct FileBlockInfo {
    uint64_t Index;
    uint64_t PrevBlock;
    uint64_t NextBlock;
    std::vector<uint8_t> Data;
  };

  static const uint32_t kFormatMagicWord = 0x43235634;
  const size_t kMaxFileBlocks = 1000; //!< Предельное ограничение на очень длинное описание файла (в блоках)

  uint16_t tag_record_size_; //!< Размер записи с информацией о тэге
  uint16_t tags_max_amount_; //!< Максимальное количество тэгов в файловой системе
  uint16_t fileblock_size_; //!< Размер файлового блока (подробнее см. формат хранения)
  uint64_t fileblock_amount_; //!< Количество файловых блоков

  uint64_t tag_table_pos_;
  uint64_t file_table_pos_;

  std::map<uint16_t, TagInfo> tags_;
  std::map<uint64_t, FileBlockInfo> file_blocks_;
  std::map<uint64_t, FileInfo> files_;

  /*! Подчистим все поля, выставим дефалтовые значения */
  void ClearAll();

  bool ReadHeader(std::ifstream& file);
  bool ReadTags(std::ifstream& file);
  bool ReadFiles(std::ifstream& file);
  bool ComposeFiles();

};


#endif // TAG_FILE_43235634_H
