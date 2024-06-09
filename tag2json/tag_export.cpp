#include "tag_export.h"

#include <fstream>
#include <iostream>
#include <string>

#include "tag_file.h"

#include "../libs/json.hpp"


using json = nlohmann::json;

int TagExport(const std::string& src, const std::string& dst, bool rewrite) {
  std::ifstream sf(src, std::ios_base::in | std::ios_base::binary);
  if (!sf) {
    std::cerr << "Can't open source file '" << src << "'" << std::endl;
    return 1;
  }

  auto tf = GetReader(sf);
  if (!tf) { return 1; }

  if (!rewrite) {
    std::ifstream def(dst, std::ios_base::in);
    if (def) {
      std::cerr << "Target file '" << dst <<
          "' is existed now. For overwriting use option '-f'" << std::endl;
      return 1;
    }
  }

  // Json stub
  json j = { {"settings", nullptr}, {"tags", nullptr}, {"files", nullptr} };
  // TagVfs settings
  j["settings"]["tagrecordsize"] = tf->GetTagRecordSize();
  j["settings"]["maxtags"] = tf->GetTagMaxAmount();
  j["settings"]["fileblocksize"] = tf->GetFileBlockSize();

  tf->EnumTags([&j](uint16_t, const std::string& name){
    j["tags"].push_back({{"name", name}});
  });

  tf->EnumFiles([&tf, &j](const std::string& name, const std::string& target,
      const uint16_t* tags, size_t tags_amount){
    json tj;
    for (size_t tc = 0; tc < tags_amount; ++tc) {
      std::string tname;
      if (!tf->GetTagInfo(tags[tc], tname)) {
        std::cerr << "Can't tag with index " << tc << std::endl;
        continue;
      }
      tj.push_back(tname);
    }
    j["files"].push_back({{"name", name}, {"target", target}, {"tags", tj}});
  });

  std::ofstream df(dst, std::ios_base::out | std::ios_base::trunc);
  if (!df) {
    std::cerr << "Can't create/open file '" << dst << "' for writing" << std::endl;
    return 1;
  }

  df << std::setw(2) << j;

  return 0;
}
