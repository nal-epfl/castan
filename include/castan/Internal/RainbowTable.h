#ifndef CASTAN_INTERNAL_RAINBOWTABLE_H
#define CASTAN_INTERNAL_RAINBOWTABLE_H

#include <string>
#include <vector>
#include <fstream>

namespace castan {
class RainbowTable {
private:
  std::ifstream file;
  unsigned lineLength;
  uint64_t value;

public:
  RainbowTable(std::string filename, uint64_t value);
  std::vector<uint8_t> getValue();
};
}

#endif
