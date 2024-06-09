#include "tag_file.h"

#include <iostream>

#include <boost/endian.hpp>

#include "formats/tag_file_43235634.h"

namespace be = boost::endian;

std::shared_ptr<TagFileReader> GetReader(std::ifstream& file) {
  try {
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.seekg(0);
    uint32_t f32;
    file.read((char*)&f32, sizeof(f32)); // magic word
    auto frm = be::big_to_native(f32);
    if (frm == 0x43235634) {
      auto tf = std::make_shared<TagFileReader43235634>();
      if (tf->Load(file)) { return tf; }
    } else {
      std::cerr << "Unknown file format" << std::endl;
    }
  } catch (std::exception& e) {
    std::cerr << "File error: " << e.what() << std::endl;
  }
  return std::shared_ptr<TagFileReader>();
}
