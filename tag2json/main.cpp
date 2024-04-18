#include <iostream>

void PrintHelp() {
  std::cout << "tag2json utility for import/export information about tags in the json format. Evgeny Kislov, 2024" << std::endl;
  std::cout << "Usage:" << std::endl;
  std::cout << "  tag2json [-e tagfile jsonfile] [-i jsonfile tagfile] [-f] [--help|-h] [--version]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -e tagfile jsonfile export tag information from tagvfs file to json file" << std::endl;
  std::cout << "  -i jsonfile tagfile import tag information from json file to tagvfs file" << std::endl;
  std::cout << "  -f force rewrite output file, without confirmation" << std::endl;
  std::cout << "  -h, --help show help" << std::endl;
  std::cout << "  -version show version information" << std::endl;
}

int main()
{
  PrintHelp();
  return 0;
}
