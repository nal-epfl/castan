#include <castan/Internal/GenericCacheModel.h>

#include <fstream>

#include "../Core/TimingSolver.h"
#include "klee/CommandLine.h"
#include "klee/Internal/Support/Debug.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include <klee/Internal/Module/InstructionInfoTable.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/DebugInfo.h>
#include <llvm/IR/Instruction.h>

static struct {
  unsigned int size;          // bytes
  char writeBack;             // 0 = write-through; 1 = write-back.
  double latency;             // cycles (if hit).
  unsigned int associativity; // ways; 0 = use contention set file
  std::string contentionSetFile;
  // [<[addresses], associativity>]
  std::vector<std::pair<std::set<long>, unsigned int>> contentionSets;
  std::map<long, long> contentionSetIdx;
} cacheConfig[] = {
#if CACHE_NUM_LAYERS >= 1
    {CACHE_L1_SIZE,
     CACHE_L1_WRITEBACK,
     CACHE_L1_LATENCY,
     CACHE_L1_ASSOCIATIVITY,
     "",
     {},
     {}},
#endif
#if CACHE_NUM_LAYERS >= 2
    {CACHE_L2_SIZE,
     CACHE_L2_WRITEBACK,
     CACHE_L2_LATENCY,
     CACHE_L2_ASSOCIATIVITY,
     "",
     {},
     {}},
#endif
#if CACHE_NUM_LAYERS >= 3
    {CACHE_L3_SIZE,
     CACHE_L3_WRITEBACK,
     CACHE_L3_LATENCY,
     0,
     CACHE_L3_CONTENTIONSETS,
     {},
     {}},
#endif
    {0, 0, CACHE_DRAM_LATENCY, 0, "", {}, {}},
};

namespace castan {
llvm::cl::opt<bool> WorstCaseSymIndices(
    "worst-case-sym-indices", llvm::cl::init(false),
    llvm::cl::desc("Pick values for symbolic indices that exercise worst case "
                   "cache scenarios (default=off)"));

llvm::cl::opt<unsigned>
    MaxLoops("max-loops", llvm::cl::init(0),
             llvm::cl::desc("Maximum number of loops to explore "
                            "(default: until cache state loops)"));

GenericCacheModel::GenericCacheModel() {
  for (unsigned int level = 0; cacheConfig[level].size; level++) {
    if (cacheConfig[level].associativity) {
      klee::klee_message(
          "Modeling L%d cache of %d kiB as %d-way associative, %s.", level + 1,
          cacheConfig[level].size / 1024, cacheConfig[level].associativity,
          cacheConfig[level].writeBack ? "write-back" : "write-through");
    } else {
      klee::klee_message(
          "Modeling L%d cache of %d kiB using contention sets in %s, %s.",
          level + 1, cacheConfig[level].size / 1024,
          cacheConfig[level].contentionSetFile.c_str(),
          cacheConfig[level].writeBack ? "write-back" : "write-through");

      std::ifstream inFile(cacheConfig[level].contentionSetFile);
      assert(inFile.good());

      // Load contention sets from files.
      while (inFile.good()) {
        std::string line;
        std::getline(inFile, line);
        if (line.empty()) {
          continue;
        }

        int associativity = std::stoi(line);
        std::set<long> addresses;
        while (inFile.good() && (std::getline(inFile, line), !line.empty())) {
          addresses.insert(std::stol(line));
        }
        cacheConfig[level].contentionSets.push_back(
            std::make_pair(addresses, associativity));
      }

      for (unsigned long i = 0; i < cacheConfig[level].contentionSets.size();
           i++) {
        for (long address : cacheConfig[level].contentionSets[i].first) {
          cacheConfig[level].contentionSetIdx[address] = i;
        }
      }
    }
  }
}

void GenericCacheModel::updateCache(uint64_t address, bool isWrite,
                                    uint8_t level) {
  // Check if accessing beyond last cache (DRAM).
  if (!cacheConfig[level].size) {
    loopStats.back().hitCount[level]++;
    //             klee::klee_message("  DRAM Access at address: %ld.",
    //             address);
    return;
  }

  uint64_t blockPtr = address >> BLOCK_BITS;
  int64_t line = -1;
  unsigned int associativity = UINT_MAX;
  if (cacheConfig[level].associativity) {
    line = blockPtr % (cacheConfig[level].size /
                       cacheConfig[level].associativity / (1 << BLOCK_BITS));
    associativity = cacheConfig[level].associativity;
  } else {
    if (cacheConfig[level].contentionSetIdx.count(blockPtr << BLOCK_BITS)) {
      long setIdx = cacheConfig[level].contentionSetIdx[blockPtr << BLOCK_BITS];
      line = *cacheConfig[level].contentionSets[setIdx].first.begin();
      associativity = cacheConfig[level].contentionSets[setIdx].second;
    }
  }
  //   klee::klee_message("  L%d access at address: %ld, line: %d/%d",
  //                      level+1, address, line,
  //                      (cacheConfig[level].size /
  //                      associativity /
  //                   (1 << BLOCK_BITS)));

  // Advance time counter for LRU algorithm.
  if (level == 0) {
    currentTime++;
  }

  // Check if cache hit.
  if (cache[level][line].count(blockPtr)) {
    if (isWrite && !cacheConfig[level].writeBack) {
      // Write-through to next level.
      updateCache(address, isWrite, level + 1);
    } else {
      // Write-back or read, don't propagate deeper.
      loopStats.back().hitCount[level]++;
    }

    // Update use time.
    cache[level][line][blockPtr].useTime = currentTime;
    // Read hit doesn't affect dirtiness.
    if (isWrite) {
      cache[level][line][blockPtr].dirty = cacheConfig[level].writeBack;
    }
    //             klee::klee_message("  L%d Hit.", level + 1);
    return;
  }

  // Cache miss.
  // Check if an old entry must be evicted.
  if (cache[level][line].size() >= associativity) {
    // Find oldest entry in cache line.
    uint64_t lruPtr = cache[level][line].begin()->first;
    for (auto entry : cache[level][line]) {
      if (entry.second.useTime < cache[level][line][lruPtr].useTime) {
        lruPtr = entry.first;
      }
    }
    // Write out if dirty.
    if (cache[level][line][lruPtr].dirty) {
      //             klee::klee_message("  L%d Dirty Eviction.", level + 1);
      // Write dirty evicted entry to next level.
      updateCache(lruPtr << BLOCK_BITS, 1, level + 1);
    } else {
      //             klee::klee_message("  L%d Clean Eviction.", level + 1);
    }
    cache[level][line].erase(lruPtr);
  }

  if ((!isWrite) || !cacheConfig[level].writeBack) {
    // Read in or write-through new entry from next level.
    updateCache(address, isWrite, level + 1);
  }

  cache[level][line][blockPtr].useTime = currentTime;
  cache[level][line][blockPtr].dirty = isWrite && cacheConfig[level].writeBack;
}

unsigned long GenericCacheModel::getCost(uint64_t address, bool isWrite,
                                         uint8_t level) {
  // Check if accessing beyond last cache (DRAM).
  if (!cacheConfig[level].size) {
    return cacheConfig[level].latency;
  }

  uint64_t blockPtr = address >> BLOCK_BITS;
  uint32_t line;
  unsigned int associativity = UINT_MAX;
  if (cacheConfig[level].associativity) {
    line = blockPtr % (cacheConfig[level].size /
                       cacheConfig[level].associativity / (1 << BLOCK_BITS));
    associativity = cacheConfig[level].associativity;
  } else {
    if (cacheConfig[level].contentionSetIdx.count(blockPtr << BLOCK_BITS)) {
      long setIdx = cacheConfig[level].contentionSetIdx[blockPtr << BLOCK_BITS];
      line = *cacheConfig[level].contentionSets[setIdx].first.begin();
      associativity = cacheConfig[level].contentionSets[setIdx].second;
    }
  }

  // Check if cache hit.
  if (cache[level][line].count(blockPtr)) {
    if (isWrite && !cacheConfig[level].writeBack) {
      // Write-through to next level.
      return getCost(address, isWrite, level + 1);
    } else {
      // Write-back or read, don't propagate deeper.
      return cacheConfig[level].latency;
    }
  }

  // Cache miss.
  // Check if an old entry must be evicted.
  unsigned long cost = 0;
  if (cache[level][line].size() >= associativity) {
    // Find oldest entry in cache line.
    uint64_t lruPtr = cache[level][line].begin()->first;
    for (auto entry : cache[level][line]) {
      if (entry.second.useTime < cache[level][line][lruPtr].useTime) {
        lruPtr = entry.first;
      }
    }
    // Write out if dirty.
    if (cache[level][line][lruPtr].dirty) {
      // Write dirty evicted entry to next level.
      cost += getCost(lruPtr << BLOCK_BITS, 1, level + 1);
    }
  }

  if ((!isWrite) || !cacheConfig[level].writeBack) {
    // Read in or write-through new entry from next level.
    cost += getCost(address, isWrite, level + 1);
  } else {
    // Write-back to current level.
    cost += cacheConfig[level].latency;
  }

  return cost;
}

unsigned long GenericCacheModel::getMissCost(uint64_t address, bool isWrite,
                                             uint8_t level) {
  // Check if accessing beyond last cache (DRAM).
  if (!cacheConfig[level].size) {
    return cacheConfig[level].latency;
  }

  uint64_t blockPtr = address >> BLOCK_BITS;
  uint32_t line;
  unsigned int associativity = UINT_MAX;
  if (cacheConfig[level].associativity) {
    line = blockPtr % (cacheConfig[level].size /
                       cacheConfig[level].associativity / (1 << BLOCK_BITS));
  } else {
    if (cacheConfig[level].contentionSetIdx.count(blockPtr << BLOCK_BITS)) {
      long setIdx = cacheConfig[level].contentionSetIdx[blockPtr << BLOCK_BITS];
      line = *cacheConfig[level].contentionSets[setIdx].first.begin();
      associativity = cacheConfig[level].contentionSets[setIdx].second;
    }
  }

  // Check if an old entry must be evicted.
  unsigned long cost = 0;
  if (cache[level][line].size() >= associativity) {
    // Find oldest entry in cache line.
    uint64_t lruPtr = cache[level][line].begin()->first;
    for (auto entry : cache[level][line]) {
      if (entry.second.useTime < cache[level][line][lruPtr].useTime) {
        lruPtr = entry.first;
      }
    }
    // Write out if dirty.
    if (cache[level][line][lruPtr].dirty) {
      // Write dirty evicted entry to next level.
      cost += getCost(lruPtr << BLOCK_BITS, 1, level + 1);
    }
  }

  if ((!isWrite) || !cacheConfig[level].writeBack) {
    // Read in or write-through new entry from next level.
    cost += getMissCost(address, isWrite, level + 1);
  } else {
    // Write-back to current level.
    cost += cacheConfig[level].latency;
  }

  return cost;
}

unsigned long GenericCacheModel::getMissesUntilEviction(uint64_t address) {
  int count = 0;
  klee::klee_message("Checking how many misses would be needed for an eviction "
                      "for addresses like %08lX.",
                      address);
//   for (uint8_t level = 0; cacheConfig[level].size; level++) {
//     if (cacheConfig[level].associativity) {
//       uint32_t numLines = cacheConfig[level].size /
//                           cacheConfig[level].associativity / (1 << BLOCK_BITS);
//       //       klee::klee_message(
//       //           "  %ld misses at L%d.",
//       //           cacheConfig[level].associativity -
//       //               cache[level][(address >> BLOCK_BITS) % numLines].size(),
//       //           level + 1);
//       count += cacheConfig[level].associativity -
//                cache[level][(address >> BLOCK_BITS) % numLines].size();
//     } else {
      int level=2;
      if (cacheConfig[level].contentionSetIdx.count(address)) {
        long setIdx = cacheConfig[level].contentionSetIdx[address];
        klee::klee_message("  %ld misses at L%d.",
                            cacheConfig[level].contentionSets[setIdx].second
                            -
                                cache[level][address].size(),
                            level + 1);
        count += cacheConfig[level].contentionSets[setIdx].second -
                 cache[level][address].size();
      } else {
        klee::klee_message("  Not in a contention set.");
        // Worst case scenario.
        count += CACHE_L3_SIZE / (1 << BLOCK_BITS);
      }
//     }
//   }

  //   klee::klee_message("  %d misses total.", count);
  return count;
}

klee::ref<klee::Expr> GenericCacheModel::memoryOperation(
    klee::Executor *executor, klee::ExecutionState &state,
    klee::ref<klee::Expr> address, bool isWrite) {
  //       klee::klee_message("Memory %s at %s:%d.", isWrite ? "write" : "read",
  //                          state.pc->info->file.c_str(),
  //                          state.pc->info->line);

  if (!isa<klee::ConstantExpr>(address)) {
    if (llvm::MDNode *node = state.pc->inst->getMetadata("dbg")) {
      llvm::DILocation loc(node);
      klee::klee_message("  Symbolic pointer at %s:%d",
                         loc.getFilename().str().c_str(), loc.getLineNumber());
    } else {
      klee::klee_message("  Symbolic pointer.");
    }
    //     state.dumpStack(llvm::outs());

    address = state.constraints.simplifyExpr(address);
    klee::klee_message("    Expr:");
    address->dump();

    bool found = false;
    if (WorstCaseSymIndices) {
      // Symbolic pointer, may hold several values: try the worst case scenarios
      // in turn until the constraints are SAT.
      // Find the maximum line granularity of all levels.
      std::set<uint32_t> lines;
      bool enumeratedSets = false;
      for (uint8_t level = 0; cacheConfig[level].size; level++) {
        if (cacheConfig[level].associativity) {
          uint32_t numLines = cacheConfig[level].size /
                              cacheConfig[level].associativity /
                              (1 << BLOCK_BITS);
          for (uint32_t line = 0; line < numLines; line++) {
            lines.insert(line);
          }
        } else {
          for (auto set : cacheConfig[level].contentionSets) {
            lines.insert((*set.first.begin()) >> BLOCK_BITS);
          }
          enumeratedSets = true;
        }
      }
      // Sort lines by how much damage a miss would cause.
      // Randomize among equal candidates.
      srand(time(NULL));
      // [<<-cycles, - misses till eviction>, rand>] -> line.
      std::map<std::pair<std::pair<long, long>, int>, uint64_t> lineCosts;
      for (uint32_t line : lines) {
        lineCosts[std::make_pair(
            std::make_pair(-getMissCost(line << BLOCK_BITS, isWrite, 0),
                           getMissesUntilEviction(line << BLOCK_BITS)),
            rand())] = line;
      }

      klee::klee_message(
          "%s symbolic pointer. Checking %ld cache lines with "
          "costs ranging %ld-%ld cycles and %ld-%ld misses until eviction.",
          isWrite ? "Writing" : "Reading", lineCosts.size(),
          -lineCosts.rbegin()->first.first.first,
          -lineCosts.begin()->first.first.first,
          lineCosts.begin()->first.first.second,
          lineCosts.rbegin()->first.first.second);
      for (auto line : lineCosts) {
        klee::klee_message("Trying line %ld with cost %ld cycles and %ld "
                           "misses until eviction.",
                           line.second, -line.first.first.first,
                           line.first.first.second);

        // Generate constraints on address that would make the miss happen.
        if (enumeratedSets) {
          // Get all cache hit entries.
          std::set<long> addresses, hits;
          unsigned int associativity = 0;
          for (uint8_t level = 0; cacheConfig[level].size; level++) {
            if (cacheConfig[level].associativity) {
              uint32_t numLines = cacheConfig[level].size /
                                  cacheConfig[level].associativity /
                                  (1 << BLOCK_BITS);
              for (auto hitAddress : cache[level][line.second % numLines]) {
                hits.insert(hitAddress.first);
              }
              if (cacheConfig[level].associativity > associativity) {
                associativity = cacheConfig[level].associativity;
              }
            } else {
              for (auto hitAddress : cache[level][line.second]) {
                hits.insert(hitAddress.first);
              }

              // Assume for now that only one cache level is sliced.
              if (cacheConfig[level].contentionSetIdx.count(line.second
                                                            << BLOCK_BITS)) {
                long setIdx = cacheConfig[level]
                                  .contentionSetIdx[line.second << BLOCK_BITS];
                addresses = cacheConfig[level].contentionSets[setIdx].first;
                if (cacheConfig[level].contentionSets[setIdx].second >
                    associativity) {
                  associativity =
                      cacheConfig[level].contentionSets[setIdx].second;
                }
              }
            }
          }

          klee::klee_message("  Cache line belongs to %d-way slice with %ld "
                             "addresses, of which %ld are already hits.",
                             associativity, addresses.size(), hits.size());
          unsigned int hitCount = hits.size();
          for (long a : addresses) {
            klee::klee_message("    Trying address %08lX.", a);
            if (hits.count(a)) {
              klee::klee_message("      Already a hit.");
              continue;
            }

            // Constrain cache line:
            // a == address & ((1<<PAGE_BITS)-1) & ~((1<<BLOCK_BITS)-1)
            klee::ConstraintManager constraints(state.constraints);
            klee::ref<klee::Expr> e =
                constraints.simplifyExpr(klee::EqExpr::create(
                    klee::ConstantExpr::create(a, address->getWidth()),
                    klee::AndExpr::create(
                        klee::ConstantExpr::create(((1 << PAGE_BITS) - 1) &
                                                       ~((1 << BLOCK_BITS) - 1),
                                                   address->getWidth()),
                        address)));

            klee::ref<klee::ConstantExpr> ce = dyn_cast<klee::ConstantExpr>(e);
            if ((!ce.isNull()) && ce->isFalse()) {
              //               klee::klee_message("      Trivially UNSAT.");
              continue;
            }

            constraints.addConstraint(e);

            klee::ref<klee::ConstantExpr> concreteAddress;
            if (executor->solver->solver->getValue(klee::Query(constraints, address),
                                         concreteAddress)) {
              //               klee::klee_message("Line fits constraints.");

              hitCount++;
              klee::klee_message("    Found potential hit, %d more needed.",
                                 associativity - hitCount + 1);
              if (hitCount > associativity) {
                state.addConstraint(
                    klee::EqExpr::create(concreteAddress, address));
                address = concreteAddress;
                found = true;
                break;
              }
            } else {
              //               klee::klee_message("      UNSAT.");
            }
          }
          if (found) {
            break;
          }
        } else {
          // Constrain cache line:
          // (line<<BLOCK_BITS) == ((maxLines-1)<<BLOCK_BITS & address)
          klee::ConstraintManager constraints(state.constraints);
          klee::ref<klee::Expr> e =
              constraints.simplifyExpr(klee::EqExpr::create(
                  klee::ConstantExpr::create(line.second << BLOCK_BITS,
                                             address->getWidth()),
                  klee::AndExpr::create(
                      klee::ConstantExpr::create(
                          (*lines.rbegin()) << BLOCK_BITS, address->getWidth()),
                      address)));

          klee::ref<klee::ConstantExpr> ce = dyn_cast<klee::ConstantExpr>(e);
          if ((!ce.isNull()) && ce->isFalse()) {
            continue;
          }

          constraints.addConstraint(e);

          // Constrain against hit addresses.
          bool unsat = false;
          for (uint8_t level = 0; cacheConfig[level].size; level++) {
            uint32_t numLines = cacheConfig[level].size /
                                cacheConfig[level].associativity /
                                (1 << BLOCK_BITS);
            for (auto hitAddress : cache[level][line.second % numLines]) {
              // (! (hit<<BLOCK_BITS == ((-1)<<BLOCK_BITS) & address))
              klee::ref<klee::Expr> e = constraints.simplifyExpr(
                  klee::NotExpr::create(klee::EqExpr::create(
                      klee::ConstantExpr::create(hitAddress.first << BLOCK_BITS,
                                                 address->getWidth()),
                      klee::AndExpr::create(
                          klee::ConstantExpr::create((-1) << BLOCK_BITS,
                                                     address->getWidth()),
                          address))));

              klee::ref<klee::ConstantExpr> ce =
                  dyn_cast<klee::ConstantExpr>(e);
              if ((!ce.isNull()) && ce->isFalse()) {
                unsat = true;
                break;
              }

              constraints.addConstraint(e);
            }
            if (unsat) {
              break;
            }
          }
          if (unsat) {
            continue;
          }

          klee::ref<klee::ConstantExpr> concreteAddress;
          if (executor->solver->solver->getValue(klee::Query(constraints, address),
                                       concreteAddress)) {
            klee::klee_message("Line fits constraints.");
            state.addConstraint(klee::EqExpr::create(concreteAddress, address));
            address = concreteAddress;
            found = true;
            break;
          }
        }
      }
    }

    // If unable to find a good candidate for worst case cache performance,
    // just concretize the address.
    if (!found) {
      klee::klee_message("Concretizing address without worst-case analysis.");
      klee::ref<klee::ConstantExpr> concreteAddress;
      assert(executor->solver->getValue(state, address, concreteAddress) &&
             "Failed to concretize symbolic address.");
      state.constraints.addConstraint(
          klee::EqExpr::create(concreteAddress, address));
      address = concreteAddress;
    }
  }

  updateCache(dyn_cast<klee::ConstantExpr>(address)->getZExtValue(), isWrite,
              0);
  return address;
}

bool GenericCacheModel::loop(klee::ExecutionState &state) {
  if (enabled) {
    klee::klee_message("Processing iteration %ld.", loopStats.size());
    //     klee::klee_message("Cache after iteration %ld:", loopStats.size());
    //     for (auto level : cache) {
    //       klee::klee_message("  L%d (%d lines):", level.first + 1,
    //                          cacheConfig[level.first].size /
    //                              cacheConfig[level.first].associativity /
    //                              (1 << BLOCK_BITS));
    //       for (auto line : level.second) {
    //         klee::klee_message("    Line %d (%ld/%d ways):", line.first,
    //                            line.second.size(),
    //                            cacheConfig[level.first].associativity);
    //         for (auto way : line.second) {
    //           klee::klee_message(
    //               "      Address: 0x%016lx, Dirty: %d, Accessed %ld units
    //                   ago.",
    //                   way.first
    //                   << BLOCK_BITS,
    //               way.second.dirty, currentTime - way.second.useTime);
    //         }
    //       }
    //     }

    if (loopStats.size() >= MaxLoops) {
      //       klee::klee_message("Exhausted loop count.");
      return false;
    }

    loopStats.emplace_back();
    return true;
  } else {
    enabled = 1;
    loopStats.emplace_back();
    return true;
  }
}

void GenericCacheModel::exec(klee::ExecutionState &state) {
  if (enabled) {
    loopStats.back().instructionCount++;
  }
}

double GenericCacheModel::getTotalTime() {
  double ns = 0;
  for (auto it : loopStats) {
    ns += FIXED_OVERHEAD_NS + it.instructionCount * NS_PER_INSTRUCTION;
    for (auto h : it.hitCount) {
      ns += h.second * cacheConfig[h.first].latency;
    }
  }
  return ns;
}

std::string GenericCacheModel::dumpStats() {
  std::stringstream stats;

  for (unsigned i = 0; i < loopStats.size(); i++) {
    stats << "Loop Iteration " << i << "\n";
    stats << "  Instructions: " << loopStats[i].instructionCount << "\n";
    stats << "  Reads: " << loopStats[i].readCount << "\n";
    stats << "  Writes: " << loopStats[i].writeCount << "\n";
    double ns =
        FIXED_OVERHEAD_NS + loopStats[i].instructionCount * NS_PER_INSTRUCTION;
    for (auto h : loopStats[i].hitCount) {
      if (cacheConfig[h.first].size) {
        stats << "  L" << (h.first + 1) << " Hits: " << h.second << "\n";
      } else {
        stats << "  DRAM Accesses: " << h.second << "\n";
      }
      ns += h.second * cacheConfig[h.first].latency;
    }
    stats << "  Estimated Execution Time: " << ns << " ns\n";
    if (ns) {
      stats << "  Estimated Throughput (Single Core): " << (1e3 / ns)
            << "Mpps\n";
    }
  }

  return stats.str();
}
}
