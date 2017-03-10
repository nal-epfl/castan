#ifndef CASTAN_INTERNAL_GENERICCACHEMODEL_H
#define CASTAN_INTERNAL_GENERICCACHEMODEL_H

#include <castan/Internal/CacheModel.h>

#include <klee/Solver.h>

typedef struct {
  // The time of the most recent use.
  unsigned long useTime;
  // Whether it is dirty or not (for write-back).
  char dirty;
} cache_entry_t;

typedef struct {
  unsigned long instructionCount;
  unsigned long readCount;
  unsigned long writeCount;
  // [level] -> # hits
  std::map<uint8_t, unsigned long> hitCount;
} loop_stats_t;

namespace castan {
class GenericCacheModel : public CacheModel {
private:
  int enabled = 0;
  unsigned iteration = 0;

  // [level][line][address] -> cache entry
  std::map<uint8_t, std::map<uint32_t, std::map<uint64_t, cache_entry_t>>>
      cache;
  unsigned long currentTime = 0;

  // [iteration] -> stats
  std::vector<loop_stats_t> loopStats;

  void updateCache(uint64_t address, bool isWrite, uint8_t level);
  unsigned long getCost(uint64_t address, bool isWrite, uint8_t level);
  unsigned long getMissCost(uint64_t address, bool isWrite, uint8_t level);

  klee::ref<klee::Expr> memoryOperation(klee::TimingSolver *solver,
                                        klee::ExecutionState &state,
                                        klee::ref<klee::Expr> address,
                                        bool isWrite);

public:
  GenericCacheModel();
  GenericCacheModel(const GenericCacheModel &other) {}

  CacheModel *clone() { return new GenericCacheModel(*this); }

  klee::ref<klee::Expr> load(klee::TimingSolver *solver,
                             klee::ExecutionState &state,
                             klee::ref<klee::Expr> address) {
    if (enabled) {
      loopStats.back().readCount++;
      return memoryOperation(solver, state, address, false);
    } else {
      return address;
    }
  }
  klee::ref<klee::Expr> store(klee::TimingSolver *solver,
                              klee::ExecutionState &state,
                              klee::ref<klee::Expr> address) {
    if (enabled) {
      loopStats.back().writeCount++;
      return memoryOperation(solver, state, address, true);
    } else {
      return address;
    }
  }
  void exec(klee::ExecutionState &state);
  bool loop(klee::ExecutionState &state);

  long getTotalCycles();
  int getNumIterations() { return loopStats.size(); }

  std::string dumpStats();
};
}

#endif
