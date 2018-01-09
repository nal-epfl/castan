#ifndef CASTAN_INTERNAL_CONTENTIONSETCACHEMODEL_H
#define CASTAN_INTERNAL_CONTENTIONSETCACHEMODEL_H

#include <castan/Internal/CacheModel.h>

// Intel(R) Xeon(R) CPU E5 - 2667 v2
#define CYCLE .23
#define CACHE_SIZE (25600 * 1024)
#define CACHE_CONTENTIONSETS "XeonE52667v2.dat"
#define CACHE_WRITEBACK 1
#define CACHE_HIT_LATENCY (30 * CYCLE)
#define CACHE_MISS_LATENCY 62
#define NS_PER_INSTRUCTION 0.05
#define NS_PER_MEMORY_INSTRUCTION (NS_PER_INSTRUCTION + CACHE_L1_LATENCY)
#define FIXED_OVERHEAD_NS 0
#define BLOCK_BITS 6
#define PAGE_BITS 30

typedef struct {
  // The time of the most recent use.
  unsigned long useTime;
  // Whether it is dirty or not (for write-back).
  char dirty;
} contentionset_cache_entry_t;

typedef struct {
  unsigned long instructionCount;
  unsigned long readCount;
  unsigned long writeCount;
  unsigned long hitCount;
  unsigned long missCount;
} contentionset_loop_stats_t;

namespace castan {
class ContentionSetCacheModel : public CacheModel {
public:
  int enabled = 0;

  // [set-idx][address] -> cache entry
  std::map<long, std::map<uint64_t, contentionset_cache_entry_t>> cache;
  unsigned long currentTime = 0;

  // [iteration] -> stats
  std::vector<contentionset_loop_stats_t> loopStats;

  void updateCache(uint64_t address, bool isWrite);
  unsigned long getMissCost(int setIdx, bool isWrite);

  klee::ref<klee::Expr> memoryOperation(klee::Executor *executor,
                                        klee::ExecutionState &state,
                                        klee::ref<klee::Expr> address,
                                        bool isWrite);

public:
  ContentionSetCacheModel();
  ContentionSetCacheModel(const ContentionSetCacheModel &other)
      : enabled(other.enabled), cache(other.cache),
        currentTime(other.currentTime), loopStats(other.loopStats) {}

  CacheModel *clone() { return new ContentionSetCacheModel(*this); }

  klee::ref<klee::Expr> load(klee::Executor *executor,
                             klee::ExecutionState &state,
                             klee::ref<klee::Expr> address) {
    if (enabled) {
      loopStats.back().readCount++;
      return memoryOperation(executor, state, address, false);
    } else {
      return address;
    }
  }
  klee::ref<klee::Expr> store(klee::Executor *executor,
                              klee::ExecutionState &state,
                              klee::ref<klee::Expr> address) {
    if (enabled) {
      loopStats.back().writeCount++;
      return memoryOperation(executor, state, address, true);
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
