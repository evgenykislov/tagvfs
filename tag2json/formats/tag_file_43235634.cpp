#include "tag_file_43235634.h"

#include <cassert>
#include <iostream>
#include <utility>
#include <vector>

#include <boost/endian.hpp>

namespace be = boost::endian;


TagFileReader43235634::TagFileReader43235634() {
  ClearAll();
}

bool TagFileReader43235634::Load(std::ifstream& file) {
  ClearAll();

  if (!ReadHeader(file)) { return false; }
  if (!ReadTags(file)) { return false; }
  if (!ReadFiles(file)) { return false; }
  if (!ComposeFiles()) { return false; }

  return true;
}


bool TagFileReader43235634::GetTagInfo(uint16_t tag_index, std::string& name) {
  auto it = tags_.find(tag_index);
  if (it == tags_.end()) { return false; }
  name = it->second.Name;
  return true;
}


void TagFileReader43235634::EnumTags(std::function<void (uint16_t, const std::string&)> func) {
  for (auto& item: tags_) {
    func(item.first, item.second.Name);
  }
}


void TagFileReader43235634::EnumFiles(std::function<void (const std::string&, const std::string&, const uint16_t*, size_t)> func) {
  for (auto& item: files_) {
    func(item.second.FileName, item.second.TargetName, item.second.Tags.data(),
        item.second.Tags.size());
  }
}

uint16_t TagFileReader43235634::GetTagRecordSize()
{
  return tag_record_size_;
}

uint16_t TagFileReader43235634::GetTagMaxAmount()
{
  return tags_max_amount_;
}

uint16_t TagFileReader43235634::GetFileBlockSize()
{
  return fileblock_size_;
}


void TagFileReader43235634::ClearAll() {
  tag_record_size_ = 256;
  tags_max_amount_ = 64;
  fileblock_size_ = 256;
  fileblock_amount_ = 0;

  tag_table_pos_ = 0;
  file_table_pos_ = 0;

  tags_.clear();
  file_blocks_.clear();
  files_.clear();
}

bool TagFileReader43235634::ReadHeader(std::ifstream& file) {
  try {
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.seekg(0);
    uint32_t f32;
    file.read((char*)&f32, sizeof(f32)); // magic word
    if (be::big_to_native(f32) != kFormatMagicWord) {
      std::cerr << "Wrong header marker" << std::endl;
      return false;
    }
    file.read((char*)&f32, sizeof(f32)); // padding

    uint64_t f64;
    file.read((char*)&f64, sizeof(f64)); // tag table pos
    tag_table_pos_ = f64;
    file.read((char*)&f64, sizeof(f64)); // file table pos
    file_table_pos_ = f64;

    uint16_t f16;
    file.read((char*)&f16, sizeof(f16)); // tag size
    tag_record_size_ = f16;
    file.read((char*)&f16, sizeof(f16)); // tag amount
    tags_max_amount_ = f16;
    file.read((char*)&f16, sizeof(f16)); // padding
    file.read((char*)&f16, sizeof(f16)); // fileblock size
    fileblock_size_ = f16;

    file.read((char*)&f64, sizeof(f64)); // fileblock amount
    fileblock_amount_ = f64;

    return true;
  } catch (std::exception& e) {
    std::cerr << "File error: " << e.what() << std::endl;
  }

  return false;
}

bool TagFileReader43235634::ReadTags(std::ifstream& file) {
  const size_t kTagHeaderSize = 4;

  if (tag_record_size_ <= kTagHeaderSize) {
    std::cerr << "Bad file format: Tag record size is very small" << std::endl;
    return false;
  }

  try {
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.seekg(tag_table_pos_);

    size_t tag_tail_size = tag_record_size_ - kTagHeaderSize;
    std::vector<char> tail(tag_tail_size);

    for (size_t i = 0; i < tags_max_amount_; ++i) {
      uint16_t f16;
      file.read((char*)&f16, sizeof(f16)); // tag flags
      uint16_t flags = f16;
      file.read((char*)&f16, sizeof(f16)); // name length
      uint16_t len = f16;
      file.read(tail.data(), tag_tail_size);

      if (flags == 0) { continue; }
      if (len == 0 || len > tag_tail_size) {
        std::cerr << "Corrupted file: wrong length of tag name. Continue ..." << std::endl;
        continue;
      }

      TagInfo tag;
      tag.Index = i;
      tag.Flags = flags;
      tag.Name = std::string(tail.data(), tail.data() + len);
      auto res = tags_.insert({tag.Index, tag});
      assert(res.second);
    }

    return true;
  } catch (std::bad_alloc&) {
    std::cerr << "Unavailable memory" << std::endl;
  } catch (std::exception& e) {
    std::cerr << "File error: " << e.what() << std::endl;
  }

  return false;
}

bool TagFileReader43235634::ReadFiles(std::ifstream& file) {
  const size_t kFileBlockHeaderSize = 16;

  if (fileblock_size_ <= kFileBlockHeaderSize) {
    std::cerr << "Bad file format: File block size is very small" << std::endl;
    return false;
  }

  try {
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.seekg(file_table_pos_);

    size_t tail_size = fileblock_size_ - kFileBlockHeaderSize;
    std::vector<uint8_t> tail(tail_size);

    for (size_t i = 0; i < fileblock_amount_; ++i) {
      uint64_t f64;
      file.read((char*)&f64, sizeof(f64));
      uint64_t prev_block = f64;
      file.read((char*)&f64, sizeof(f64));
      uint64_t next_block = f64;
      file.read((char*)tail.data(), tail_size);

      if (prev_block == static_cast<uint64_t>(-1)) { continue; } // Free (unused) block
      if (prev_block == static_cast<uint64_t>(-2)) { continue; } // Reserved block

      FileBlockInfo fb;
      fb.Index = i;
      fb.PrevBlock = prev_block;
      fb.NextBlock = next_block;
      fb.Data = tail;
      auto res = file_blocks_.insert({fb.Index, fb});
      assert(res.second);
    }

    return true;
  } catch (std::bad_alloc&) {
    std::cerr << "Unavailable memory" << std::endl;
  } catch (std::exception& e) {
    std::cerr << "File error: " << e.what() << std::endl;
  }

  return false;
}


bool TagFileReader43235634::ComposeFiles() {
  if (file_blocks_.empty()) { return true; }
  auto cur = file_blocks_.begin()->first;
  while (true) {
    auto fb = file_blocks_.lower_bound(cur);
    if (fb == file_blocks_.end()) { break; }

    assert(fb->second.PrevBlock != static_cast<uint64_t>(-1));
    assert(fb->second.PrevBlock != static_cast<uint64_t>(-2));

    if (fb->second.PrevBlock != fb->second.Index) {
      // It's not a start block
      ++cur;
      continue;
    }

    // fb - is start chunk of file blocks
    std::vector<uint8_t> data = fb->second.Data;
    auto previ = fb->first;
    auto curi = fb->second.NextBlock;
    file_blocks_.erase(fb);
    for (size_t i = 0; i < kMaxFileBlocks; ++i) {
      if (curi == static_cast<uint64_t>(-1)) {
        std::cerr << "File blocks chain corrupted (tail cut). Continue ..." <<
            std::endl;
        break;
      }

      if (curi == previ) { break; }

      auto curfb = file_blocks_.find(curi);
      if (curfb == file_blocks_.end()) {
        std::cerr << "File blocks chain corrupted (block absent). Continue ..." <<
            std::endl;
        break;
      }
      if (curfb->second.PrevBlock != previ) {
        std::cerr << "File blocks chain corrupted (failed parent). Continue ..." <<
            std::endl;
      }

      data.insert(data.end(), curfb->second.Data.begin(), curfb->second.Data.end());
      previ = curi;
      curi = curfb->second.NextBlock;
      file_blocks_.erase(curfb);
    }

    if (data.size() < 8) {
      std::cerr << "File information is tiny" << std::endl;
      continue;
    }

    uint16_t tags_size;
    uint16_t name_size;
    uint32_t target_size;
    std::memcpy(&tags_size, data.data(), 2);
    std::memcpy(&name_size, data.data() + 2, 2);
    std::memcpy(&target_size, data.data() + 4, 4);
    if (data.size() < (8 + tags_size + name_size + target_size)) {
      std::cerr << "File information is corrupted" << std::endl;
      continue;
    }

    size_t disp = 8;
    FileInfo fi;
    for (size_t tb = 0; tb < tags_size; ++tb) {
      uint8_t bits = data[disp + tb];

      for (size_t b = 0; b < 8; ++b) {
        if (bits & 0x01) {
          fi.Tags.push_back((uint16_t)(tb * 8 + b));
        }
        bits >>= 1;
      }
    }
    disp += tags_size;
    fi.FileName = std::string(data.data() + disp, data.data() + disp + name_size);
    disp += name_size;
    fi.TargetName = std::string(data.data() + disp, data.data() + disp + target_size);
    files_[fb->first] = fi;
  }

  return true;
}
