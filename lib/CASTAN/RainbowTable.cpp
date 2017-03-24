#include <castan/Internal/RainbowTable.h>

#include <cassert>
#include <iterator>
#include <sstream>

namespace castan {
RainbowTable::RainbowTable(std::string filename, uint64_t value)
    : value(value) {
  file.open(filename);
  assert(file.good() && "Unable to open rainbow table.");

  std::string line;
  std::getline(file, line);

  lineLength = line.length() + 1;

  file.seekg(0, file.end);
  std::streampos minRecord = 0, maxRecord = file.tellg();

  assert(maxRecord % lineLength == 0 &&
         "Rainbow table has incomplete records.");

  // Binary search the file for the first record >= value.
  while (minRecord < maxRecord) {
    std::streampos midRecord =
        minRecord + (maxRecord - minRecord) / 2 / lineLength * lineLength;
    file.seekg(midRecord);

    uint64_t lineValue;
    file >> lineValue;

    if (lineValue < value) {
      if (minRecord == midRecord) {
        minRecord = midRecord + (std::streamoff)lineLength;
      } else {
        minRecord = midRecord;
      }
    } else {
      maxRecord = midRecord;
    }
  }
  file.seekg(maxRecord);
}

std::vector<uint8_t> RainbowTable::getValue() {
  if (!file.good()) {
    return std::vector<uint8_t>();
  }

  std::string line;
  std::getline(file, line);
  assert(line.length() + 1 == lineLength &&
         "Rainbow table has inconsistent records.");

  uint64_t lineValue;
  std::istringstream ss(line);
  ss >> std::hex >> lineValue;

  if (lineValue == value) {
    std::vector<uint8_t> result;
    int b;
    while (ss >> b) {
      result.push_back(b);
    }
    return result;
  } else {
    return std::vector<uint8_t>();
  }
}
}
