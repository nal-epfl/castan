#include <assert.h>
#include <bitset>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#define VIRT_ADDR_BITS 30
#define MAX_ADDR_BITS 40

int main(int argc, char **argv) {
  assert(argc >= 3 &&
         "Usage: process-contention-sets <output-set-file> <input-set-files>");
  std::ofstream outFile(argv[1]);
  assert(outFile.good());

  // Load contention sets from files.
  std::set<long> prefixes, suffixes;
  std::map<long, int> contentionSets;
  std::map<int, unsigned int> setAssociativity;
  int id = 0;
  for (int arg = 2; arg < argc; arg++) {
    std::cout << "Loading contention sets from: " << argv[arg] << std::endl;
    std::ifstream inFile(argv[arg]);
    assert(inFile.good());

    while (inFile.good()) {
      std::string line;
      std::getline(inFile, line);
      if (line.empty()) {
        continue;
      }

      setAssociativity[id] = std::stoi(line);

      long count = 0;
      while (inFile.good() && (std::getline(inFile, line), !line.empty())) {
        // Make sure addresses from different files don't overlap.
        long address = (((long)arg) << MAX_ADDR_BITS) | std::stol(line);
        prefixes.insert(address & ~((1 << VIRT_ADDR_BITS) - 1));
        suffixes.insert(address & ((1 << VIRT_ADDR_BITS) - 1));
        contentionSets[address] = id;
        count++;
      }
      std::cout << "   Loaded " << setAssociativity[id] << "-way set of "
                << count << " addresses" << std::endl;
      id++;
    }
  }

  // Find sets of addresses that are always in the same contention set
  // when in the same address prefix.
  // [<[addresses], associativity>]
  std::set<std::pair<std::set<long>, unsigned int>> sets;
  for (long addr : suffixes) {
    std::cout << "Testing: " << std::hex << addr << " / "
              << (1 << VIRT_ADDR_BITS) << std::endl;
    // If no prior sets exist, create new one with just this address.
    if (sets.empty()) {
      sets.insert(std::make_pair(std::set<long>({addr}), 1));
    }

    // Iterate over sets to see if any can be augmented with addr.
    for (auto candidate : sets) {
      // Check for each prefix if all addresses in set
      // correspond to the same contention set.
      bool found = 0;
      unsigned int maxAssociativity = 0;
      for (long prefix : prefixes) {
        if(!contentionSets.count(prefix | addr)){
          found = 1;
          break;
        }

        int set = contentionSets[prefix | addr];
        if (setAssociativity[set] > maxAssociativity) {
          maxAssociativity = setAssociativity[set];
        }

        for (long check : candidate.first) {
          if ((!contentionSets.count(prefix | check)) || contentionSets[prefix | check] != set) {
            found = 1;
            break;
          }
        }
        if (found)
          break;
      }

      if (found) { // address doesn't always match with set, create a new set.
        sets.insert(std::make_pair(std::set<long>({addr}), 1));
      } else { // Address is always consistent with set, augment set.
        sets.erase(candidate);
        candidate.first.insert(addr);
        if (maxAssociativity > candidate.second) {
          candidate.second = maxAssociativity;
        }
        //         std::cout << "Found new " << std::dec << candidate.second
        //                   << "-way set of size " << candidate.first.size() <<
        //                   std::endl;
        sets.insert(candidate);
      }
    }
  }

  // Dump sets.
  for (auto sit : sets) {
    // Only include sets that are larger than their associativity.
    if (sit.first.size() > sit.second) {
      outFile << sit.second << std::endl;
      for (long ait : sit.first) {
        outFile << ait << std::endl;
      }
      outFile << std::endl;
    }
  }

  return 0;
}
