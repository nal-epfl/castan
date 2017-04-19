#ifndef CASTAN_INTERNAL_GENERICCACHEMODEL_H
#define CASTAN_INTERNAL_GENERICCACHEMODEL_H

#include <castan/Internal/CacheModel.h>

#include <klee/Solver.h>

#define BLOCK_BITS 6

// // Fake cache.
// #define CACHE_NUM_LAYERS 3
// #define CACHE_L1_SIZE (4 << BLOCK_BITS)
// #define CACHE_L1_ASSOCIATIVITY 2
// #define CACHE_L1_WRITEBACK 1
// #define CACHE_L1_LATENCY 1
// #define CACHE_L2_SIZE (16 << BLOCK_BITS)
// #define CACHE_L2_ASSOCIATIVITY 1
// #define CACHE_L2_WRITEBACK 1
// #define CACHE_L2_LATENCY 8
// #define CACHE_L3_SIZE (64 << BLOCK_BITS)
// #define CACHE_L3_ASSOCIATIVITY 1
// #define CACHE_L3_WRITEBACK 1
// #define CACHE_L3_LATENCY 32
// #define CACHE_DRAM_LATENCY 128
// #define NS_PER_INSTRUCTION 1
// #define NS_PER_MEMORY_INSTRUCTION CACHE_L1_LATENCY

// // Fake cache with a single level.
// #define CACHE_NUM_LAYERS 1
// #define CACHE_L1_SIZE (1 << BLOCK_BITS)
// #define CACHE_L1_ASSOCIATIVITY 1
// #define CACHE_L1_WRITEBACK 1
// #define CACHE_L1_LATENCY 32
// #define CACHE_DRAM_LATENCY 128
// #define NS_PER_INSTRUCTION 1
// #define NS_PER_MEMORY_INSTRUCTION CACHE_L1_LATENCY

// // Intel(R) Core(TM) i7-2600S
// #define CYCLE (1/3.8)
// #define CACHE_NUM_LAYERS 3
// #define CACHE_L1_SIZE (32 * 1024)
// #define CACHE_L1_ASSOCIATIVITY 8
// #define CACHE_L1_WRITEBACK 1
// #define CACHE_L1_LATENCY (4 * CYCLE)
// #define CACHE_L2_SIZE (256 * 1024)
// #define CACHE_L2_ASSOCIATIVITY 8
// #define CACHE_L2_WRITEBACK 1
// #define CACHE_L2_LATENCY (10 * CYCLE)
// #define CACHE_L3_SIZE (8192 * 1024)
// #define CACHE_L3_ASSOCIATIVITY 16
// #define CACHE_L3_WRITEBACK 1
// #define CACHE_L3_LATENCY (40 * CYCLE)
// #define CACHE_DRAM_LATENCY 60
// #define NS_PER_INSTRUCTION .1
// #define NS_PER_MEMORY_INSTRUCTION (NS_PER_INSTRUCTION + CACHE_L1_LATENCY)

// Intel(R) Xeon(R) CPU E5 - 2667 v2
#define CYCLE .23
#define CACHE_NUM_LAYERS 3
#define CACHE_L1_SIZE (32 * 1024)
#define CACHE_L1_ASSOCIATIVITY 8
#define CACHE_L1_WRITEBACK 1
#define CACHE_L1_LATENCY (4 * CYCLE)
#define CACHE_L2_SIZE (256 * 1024)
#define CACHE_L2_ASSOCIATIVITY 8
#define CACHE_L2_WRITEBACK 1
#define CACHE_L2_LATENCY (12 * CYCLE)
#define CACHE_L3_SIZE (25600 * 1024)
#define CACHE_L3_ASSOCIATIVITY 20
#define CACHE_L3_WRITEBACK 1
#define CACHE_L3_LATENCY (30 * CYCLE)
#define CACHE_DRAM_LATENCY 62
#define NS_PER_INSTRUCTION 0.05
#define NS_PER_MEMORY_INSTRUCTION (NS_PER_INSTRUCTION + CACHE_L1_LATENCY)

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
  GenericCacheModel(const GenericCacheModel &other)
      : enabled(other.enabled), cache(other.cache),
        currentTime(other.currentTime), loopStats(other.loopStats) {}

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

  double getTotalTime();
  int getNumIterations() { return loopStats.size(); }

  std::string dumpStats();
};
}

#endif
