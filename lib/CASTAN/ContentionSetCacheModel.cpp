#include <castan/Internal/ContentionSetCacheModel.h>

#include <fstream>

#include "../Core/TimingSolver.h"
#include "klee/CommandLine.h"
#include "klee/Internal/Support/Debug.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include <klee/Internal/Module/InstructionInfoTable.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/DebugInfo.h>
#include <llvm/IR/Instruction.h>

// [<[addresses], associativity>]
std::vector<std::pair<std::set<long>, unsigned int>> contentionSets;
// address -> [set idxs]
std::map<long, std::set<long>> contentionSetIdxs;

namespace castan {
extern llvm::cl::opt<bool> WorstCaseSymIndices;
extern llvm::cl::opt<unsigned> MaxLoops;

llvm::cl::opt<bool> GiveUpOnComplexSymIndices(
    "give-up-on-complex-sym-indices", llvm::cl::init(false),
    llvm::cl::desc("Give up early if symbolic pointer constraints look too "
                   "complex (default=off)"));

llvm::cl::opt<bool> TerminateOnUNSAT(
    "terminate-on-unsat-sym-indices", llvm::cl::init(false),
    llvm::cl::desc("Terminate states where a symbolic pointer doesn't fit the "
                   "cache constraints (default=off)"));

ContentionSetCacheModel::ContentionSetCacheModel() {
  std::ifstream inFile(CACHE_CONTENTIONSETS);
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
    contentionSets.push_back(std::make_pair(addresses, associativity));
  }

  for (unsigned long i = 0; i < contentionSets.size(); i++) {
    for (long address : contentionSets[i].first) {
      contentionSetIdxs[address].insert(i);
    }
  }

  klee::klee_message(
      "Modeling a %s cache with %ld contention sets loaded from %s.",
      CACHE_WRITEBACK ? "write-back" : "write-through", contentionSets.size(),
      CACHE_CONTENTIONSETS);
}

void ContentionSetCacheModel::updateCache(uint64_t address, bool isWrite) {
  //   klee::klee_message("%s address %08lX.", isWrite ? "Writing" : "Reading",
  //                      address);

  uint64_t blockAddr = address & ~((1 << BLOCK_BITS) - 1);
  uint64_t pageAddr = blockAddr & ((1 << PAGE_BITS) - 1);

  std::set<long> idxs;
  if (contentionSetIdxs.count(pageAddr)) {
    idxs = contentionSetIdxs[pageAddr];
  } else {
    idxs.insert(-1);
  }
  //   klee::klee_message("  %ld contention sets affected.", idxs.size());

  // Advance time counter for LRU algorithm.
  currentTime++;

  bool miss = true, dirtyMiss = false;
  for (auto setIdx : idxs) {
    unsigned int associativity = (setIdx >= 0)
                                     ? contentionSets[setIdx].second
                                     : (CACHE_SIZE / (1 << BLOCK_BITS));
    // Check if cache hit.
    if (cache[setIdx].count(blockAddr)) {
      if (isWrite && !CACHE_WRITEBACK) {
        // Write-through.
        //         klee::klee_message("    Write-through on set id-%ld.",
        //         setIdx);
      } else {
        // Write-back or read, don't propagate deeper.
        //         klee::klee_message("    Hit on set id-%ld.", setIdx);
        miss = false;
      }

      // Update use time.
      cache[setIdx][blockAddr].useTime = currentTime;
      // Read hit doesn't affect dirtiness.
      if (isWrite) {
        cache[setIdx][blockAddr].dirty = CACHE_WRITEBACK;
      }
      continue;
    }

    // Cache miss.
    // Check if an old entry must be evicted.
    if (cache[setIdx].size() >= associativity) {
      // Find oldest entry in cache line.
      uint64_t lruPtr = cache[setIdx].begin()->first;
      for (auto entry : cache[setIdx]) {
        if (entry.second.useTime < cache[setIdx][lruPtr].useTime) {
          lruPtr = entry.first;
        }
      }
      // Write out if dirty.
      if (cache[setIdx][lruPtr].dirty) {
        // Write dirty evicted entry.
        //         klee::klee_message("    Eviction in set id-%ld.", setIdx);
        dirtyMiss = true;
      }
      cache[setIdx].erase(lruPtr);
    }

    if ((!isWrite) || !CACHE_WRITEBACK) {
      // Read in or write-through new entry from next level.
      //       klee::klee_message("    Miss in set id-%ld.", setIdx);
    }

    cache[setIdx][blockAddr].useTime = currentTime;
    cache[setIdx][blockAddr].dirty = isWrite && CACHE_WRITEBACK;
  }

  if (miss) {
    //     klee::klee_message("  Cache miss for all sets.");
    loopStats.back().missCount++;
  } else {
    //     klee::klee_message("  Cache hit for at least one set.");
    loopStats.back().hitCount++;
  }
  if (dirtyMiss) {
    //     klee::klee_message("  Eviction on at least one set.");
    loopStats.back().missCount++;
  }
}

unsigned long ContentionSetCacheModel::getMissCost(int setIdx, bool isWrite) {
  // Check if an old entry must be evicted.
  unsigned long cost = 0;
  if (cache[setIdx].size() >= contentionSets[setIdx].second) {
    // Find oldest entry in cache line.
    uint64_t lruPtr = cache[setIdx].begin()->first;
    for (auto entry : cache[setIdx]) {
      if (entry.second.useTime < cache[setIdx][lruPtr].useTime) {
        lruPtr = entry.first;
      }
    }
    // Write out if dirty.
    if (cache[setIdx][lruPtr].dirty) {
      // Write dirty evicted entry.
      cost += CACHE_MISS_LATENCY;
    }
  }

  if ((!isWrite) || !CACHE_WRITEBACK) {
    // Read in or write-through new entry.
    cost += CACHE_MISS_LATENCY;
  } else {
    // Write-back.
    cost += CACHE_HIT_LATENCY;
  }

  return cost;
}

klee::ref<klee::Expr> ContentionSetCacheModel::memoryOperation(
    klee::Executor *executor, klee::ExecutionState &state,
    klee::ref<klee::Expr> address, bool isWrite) {
  //       klee::klee_message("Memory %s at %s:%d.", isWrite ? "write" : "read",
  //                          state.pc->info->file.c_str(),
  //                          state.pc->info->line);

  address = state.constraints.simplifyExpr(address);

  if (!isa<klee::ConstantExpr>(address)) {
    if (llvm::MDNode *node = state.pc->inst->getMetadata("dbg")) {
      llvm::DILocation loc(node);
      klee::klee_message("  Symbolic pointer at %s:%d",
                         loc.getFilename().str().c_str(), loc.getLineNumber());
    } else {
      klee::klee_message("  Symbolic pointer.");
    }
    //     state.dumpStack(llvm::outs());

    klee::klee_message("    Expr:");
    address->dump();
    klee::klee_message("    Assuming:");
    for (auto c : state.constraints) {
      c->dump();
    }

    bool found = false, giveup = false;
    if (WorstCaseSymIndices) {
      // Symbolic pointer, may hold several values: try the worst case scenarios
      // in turn until the constraints are SAT.
      // Sort contention sets by how much damage a miss would cause.
      // Then sort by how many more misses in the set before an eviction.
      // Randomize among equal candidates.
      srand(time(NULL));
      // [<<-cycles, - misses till eviction>, rand>] -> setIdx.
      std::map<std::pair<std::pair<long, long>, int>, uint64_t> setCosts;
      for (unsigned int setIdx = 0; setIdx < contentionSets.size(); setIdx++) {
        setCosts[std::make_pair(std::make_pair(-getMissCost(setIdx, isWrite),
                                               (contentionSets[setIdx].second -
                                                cache[setIdx].size())),
                                rand())] = setIdx;
      }

      klee::klee_message(
          "%s symbolic pointer. Checking %ld contention sets with "
          "costs ranging %ld-%ld cycles and %ld-%ld misses until eviction.",
          isWrite ? "Writing" : "Reading", setCosts.size(),
          -setCosts.rbegin()->first.first.first,
          -setCosts.begin()->first.first.first,
          setCosts.begin()->first.first.second,
          setCosts.rbegin()->first.first.second);
      for (auto set : setCosts) {
        klee::klee_message(
            "Trying contention set id-%ld with cost %ld cycles and %ld "
            "misses until eviction.",
            set.second, -set.first.first.first, set.first.first.second);

        // Generate constraints on address that would make the miss happen.
        klee::klee_message("  Cache line belongs to %d-way slice with %ld "
                           "addresses, of which %ld are already hits.",
                           contentionSets[set.second].second,
                           contentionSets[set.second].first.size(),
                           cache[set.second].size());
        unsigned int hitCount = cache[set.second].size();
        for (long a : contentionSets[set.second].first) {
          klee::klee_message("    Trying address %08lX.", a);

          bool hit = false;
          for (auto idx : contentionSetIdxs[a]) {
            if (cache[idx].count(a)) {
              hit = true;
              break;
            }
          }
          if (hit) {
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
          if (executor->solver->solver->getValue(
                  klee::Query(constraints, address), concreteAddress)) {
            //               klee::klee_message("Line fits constraints.");

            hitCount++;
            klee::klee_message("    Found potential hit, %d more needed.",
                               contentionSets[set.second].second - hitCount +
                                   1);
            if (hitCount > contentionSets[set.second].second) {
              state.addConstraint(
                  klee::EqExpr::create(concreteAddress, address));
              address = concreteAddress;
              found = true;

              klee::klee_message(
                  "Picked address: %08lX",
                  dyn_cast<klee::ConstantExpr>(address)->getZExtValue());
              break;
            }
          } else {
            //               klee::klee_message("      UNSAT.");
            if (GiveUpOnComplexSymIndices) {
              giveup = true;
              break;
            }
          }
        }
        if (found || giveup) {
          break;
        }
      }
    }

    // If unable to find a good candidate for worst case cache performance,
    // just concretize the address.
    if (!found) {
      if (TerminateOnUNSAT) {
        executor->terminateStateEarly(state, "unable to induce cache miss.");
        return address;
      } else {
        klee::klee_message("Concretizing address without worst-case analysis.");
        klee::ref<klee::ConstantExpr> concreteAddress;
        assert(executor->solver->getValue(state, address, concreteAddress) &&
               "Failed to concretize symbolic address.");
        state.constraints.addConstraint(
            klee::EqExpr::create(concreteAddress, address));
        address = concreteAddress;
      }
    }
  }

  updateCache(dyn_cast<klee::ConstantExpr>(address)->getZExtValue(), isWrite);
  return address;
}

bool ContentionSetCacheModel::loop(klee::ExecutionState &state) {
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

void ContentionSetCacheModel::exec(klee::ExecutionState &state) {
  if (enabled) {
    loopStats.back().instructionCount++;
  }
}

double ContentionSetCacheModel::getTotalTime() {
  double ns = 0;
  for (auto it : loopStats) {
    ns += FIXED_OVERHEAD_NS + it.instructionCount * NS_PER_INSTRUCTION +
          it.hitCount * CACHE_HIT_LATENCY + it.missCount * CACHE_MISS_LATENCY;
  }
  return ns;
}

std::string ContentionSetCacheModel::dumpStats() {
  std::stringstream stats;

  for (unsigned i = 0; i < loopStats.size(); i++) {
    stats << "Loop Iteration " << i << "\n";
    stats << "  Instructions: " << loopStats[i].instructionCount << "\n";
    stats << "  Reads: " << loopStats[i].readCount << "\n";
    stats << "  Writes: " << loopStats[i].writeCount << "\n";
    stats << "  Cache Hits: " << loopStats[i].hitCount << "\n";
    stats << "  DRAM Accesses: " << loopStats[i].missCount << "\n";

    double ns = getTotalTime();
    stats << "  Estimated Execution Time: " << ns << " ns\n";
    if (ns) {
      stats << "  Estimated Throughput (Single Core): " << (1e3 / ns)
            << "Mpps\n";
    }
  }

  return stats.str();
}
}
