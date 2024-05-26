#include <cassert>
#include <iostream>
#include <string>

#include "tag_export.h"

#define TAG2JSON_VERSION "1.0"

void PrintHelp() {
  std::cout << "tag2json utility for import/export information about tags in the json format. Evgeny Kislov, 2024" << std::endl;
  std::cout << "Usage:" << std::endl;
  std::cout << "  tag2json [-e tagfile jsonfile] [-i jsonfile tagfile] [-f] [--help|-h] [--version]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -e tagfile jsonfile export tag information from tagvfs file to json file" << std::endl;
  std::cout << "  -i jsonfile tagfile import tag information from json file to tagvfs file" << std::endl;
  std::cout << "  -f force rewrite output file, without confirmation" << std::endl;
  std::cout << "  -h, --help show help and exit" << std::endl;
  std::cout << "  --version show version information and exit" << std::endl;
}

int main(int argc, char** argv) {
  bool opt_force = false;
  bool opt_export = false;
  bool opt_import = false;
  std::string opt_exp_src, opt_exp_dst;
  std::string opt_imp_src, opt_imp_dst;
  bool bad_cmd = false;
  for (int i = 1; i < argc; ++i) {
    std::string opt = argv[i];
    if (opt == "-h" || opt == "--help") {
      PrintHelp();
      return 0;
    } else if (opt == "--version") {
      std::cout << TAG2JSON_VERSION << std::endl;
      return 0;
    } else if (opt == "-f") {
      opt_force = true;
    } else if (opt == "-e") {
      if ((i + 2) < argc) {
        opt_export = true;
        opt_exp_src = argv[i + 1];
        opt_exp_dst = argv[i + 2];
        i += 2;
      } else {
        bad_cmd = true;
        break;
      }
    } else if (opt == "-i") {
      if ((i + 2) < argc) {
        opt_import = true;
        opt_imp_src = argv[i + 1];
        opt_imp_dst = argv[i + 2];
        i += 2;
      } else {
        bad_cmd = true;
        break;
      }
    } else {
      bad_cmd = true;
    }
  }
  if (bad_cmd || (opt_export && opt_import) || !(opt_export || opt_import)) {
    PrintHelp();
    return 1;
  }

  if (opt_export) {
    return TagExport(opt_exp_src, opt_exp_dst, opt_force);
  }

  if (opt_import) {
    std::cerr << "Import isn't implemented yet" << std::endl;
  }

  assert(false);
  return 1;
}
