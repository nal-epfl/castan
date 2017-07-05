#include <assert.h>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <bitset>

#define OFFSET_BITS 17
#define MAX_ADDR_BITS 30

int main(int argc, char **argv) {
  assert(argc > 1);

  // Load contention sets from files.
  std::map<long, int> contentionSets;
  for (int arg = 1; arg < argc; arg++) {
    std::cout << "Loading " << argv[arg] << " as set " << (arg - 1) << "." << std::endl;

    std::ifstream inFile(argv[arg]);
    assert(inFile.good());

    long address;
    while (inFile >> address) {
      contentionSets[address] = arg - 1;
    }
  }

  // Find sets of addresses that are always in the same contention set
  // when in the same address prefix.
  std::set<std::set<long>> sets;
  for (long addr = 0; addr < 1<<MAX_ADDR_BITS; addr += 1<<OFFSET_BITS) {
    std::cout << "Testing: " << std::hex << addr << std::endl;
    // If no prior sets exist, create new one with just this address.
    if (sets.empty()) {
      sets.insert(std::set<long>({addr}));
    }

    // Iterate over sets to see if any meet the criteria.
    for (auto candidate : sets) {
      // Check for each prefix if all addresses in set
      // correspond to the same contention set.
      bool found = 0;
      for (long prefix = 0; contentionSets.count((prefix<<MAX_ADDR_BITS) | addr); prefix++) {
        int set = contentionSets[(prefix<<MAX_ADDR_BITS) | addr];

        for (long check : candidate) {
          if (contentionSets[(prefix<<MAX_ADDR_BITS) | check] != set) {
            found = 1;
            break;
          }
        }
        if (found) break;
      }

      if (found) { // address doesn't always match with set, create a new set.
        sets.insert(std::set<long>({addr}));
      } else { // Address is always consistent with set, augment set.
        sets.erase(candidate);
        candidate.insert(addr);
        std::cout << "Found new set of size " << std::dec << candidate.size() << std::endl;
        sets.insert(candidate);
      }
    }
  }

  // Dump sets.
  for (auto sit : sets) {
    std::cout << "Set of size " << std::dec << sit.size() << ":";

    for (long ait : sit) {
      std::cout << " " << ait;
    }
    std::cout << std::endl;
  }

  return 0;
}
