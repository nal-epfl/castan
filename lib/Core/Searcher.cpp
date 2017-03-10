//===-- Searcher.cpp ------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Searcher.h"

#include "CoreStats.h"
#include "Executor.h"
#include "PTree.h"
#include "StatsTracker.h"
#include "Memory.h"

#include "castan/Internal/CacheModel.h"

#include "klee/ExecutionState.h"
#include "klee/Statistics.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/ADT/DiscretePDF.h"
#include "klee/Internal/ADT/RNG.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "klee/Internal/System/Time.h"
#include "klee/Internal/Support/ErrorHandling.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#else
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#endif
#include "llvm/Support/CommandLine.h"

#if LLVM_VERSION_CODE < LLVM_VERSION(3, 5)
#include "llvm/Support/CallSite.h"
#else
#include "llvm/IR/CallSite.h"
#endif

#include <cassert>
#include <fstream>
#include <climits>
#include <sys/stat.h>

using namespace klee;
using namespace llvm;

namespace {
  cl::opt<bool>
  DebugLogMerge("debug-log-merge");
}

namespace klee {
  extern RNG theRNG;
}

Searcher::~Searcher() {
}

///

ExecutionState &DFSSearcher::selectState() {
  return *states.back();
}

void DFSSearcher::update(ExecutionState *current,
                         const std::vector<ExecutionState *> &addedStates,
                         const std::vector<ExecutionState *> &removedStates) {
  states.insert(states.end(),
                addedStates.begin(),
                addedStates.end());
  for (std::vector<ExecutionState *>::const_iterator it = removedStates.begin(),
                                                     ie = removedStates.end();
       it != ie; ++it) {
    ExecutionState *es = *it;
    if (es == states.back()) {
      states.pop_back();
    } else {
      bool ok = false;

      for (std::vector<ExecutionState*>::iterator it = states.begin(),
             ie = states.end(); it != ie; ++it) {
        if (es==*it) {
          states.erase(it);
          ok = true;
          break;
        }
      }

      assert(ok && "invalid state removed");
    }
  }
}

///

ExecutionState &BFSSearcher::selectState() {
  return *states.front();
}

void BFSSearcher::update(ExecutionState *current,
                         const std::vector<ExecutionState *> &addedStates,
                         const std::vector<ExecutionState *> &removedStates) {
  // Assumption: If new states were added KLEE forked, therefore states evolved.
  // constraints were added to the current state, it evolved.
  if (!addedStates.empty() && current &&
      std::find(removedStates.begin(), removedStates.end(), current) ==
          removedStates.end()) {
    assert(states.front() == current);
    states.pop_front();
    states.push_back(current);
  }

  states.insert(states.end(),
                addedStates.begin(),
                addedStates.end());
  for (std::vector<ExecutionState *>::const_iterator it = removedStates.begin(),
                                                     ie = removedStates.end();
       it != ie; ++it) {
    ExecutionState *es = *it;
    if (es == states.front()) {
      states.pop_front();
    } else {
      bool ok = false;

      for (std::deque<ExecutionState*>::iterator it = states.begin(),
             ie = states.end(); it != ie; ++it) {
        if (es==*it) {
          states.erase(it);
          ok = true;
          break;
        }
      }

      assert(ok && "invalid state removed");
    }
  }
}

///

ExecutionState &RandomSearcher::selectState() {
  return *states[theRNG.getInt32()%states.size()];
}

void
RandomSearcher::update(ExecutionState *current,
                       const std::vector<ExecutionState *> &addedStates,
                       const std::vector<ExecutionState *> &removedStates) {
  states.insert(states.end(),
                addedStates.begin(),
                addedStates.end());
  for (std::vector<ExecutionState *>::const_iterator it = removedStates.begin(),
                                                     ie = removedStates.end();
       it != ie; ++it) {
    ExecutionState *es = *it;
    bool ok = false;

    for (std::vector<ExecutionState*>::iterator it = states.begin(),
           ie = states.end(); it != ie; ++it) {
      if (es==*it) {
        states.erase(it);
        ok = true;
        break;
      }
    }
    
    assert(ok && "invalid state removed");
  }
}

///

WeightedRandomSearcher::WeightedRandomSearcher(WeightType _type)
  : states(new DiscretePDF<ExecutionState*>()),
    type(_type) {
  switch(type) {
  case Depth: 
    updateWeights = false;
    break;
  case InstCount:
  case CPInstCount:
  case QueryCost:
  case MinDistToUncovered:
  case CoveringNew:
    updateWeights = true;
    break;
  default:
    assert(0 && "invalid weight type");
  }
}

WeightedRandomSearcher::~WeightedRandomSearcher() {
  delete states;
}

ExecutionState &WeightedRandomSearcher::selectState() {
  return *states->choose(theRNG.getDoubleL());
}

double WeightedRandomSearcher::getWeight(ExecutionState *es) {
  switch(type) {
  default:
  case Depth: 
    return es->weight;
  case InstCount: {
    uint64_t count = theStatisticManager->getIndexedValue(stats::instructions,
                                                          es->pc->info->id);
    double inv = 1. / std::max((uint64_t) 1, count);
    return inv * inv;
  }
  case CPInstCount: {
    StackFrame &sf = es->stack.back();
    uint64_t count = sf.callPathNode->statistics.getValue(stats::instructions);
    double inv = 1. / std::max((uint64_t) 1, count);
    return inv;
  }
  case QueryCost:
    return (es->queryCost < .1) ? 1. : 1./es->queryCost;
  case CoveringNew:
  case MinDistToUncovered: {
    uint64_t md2u = computeMinDistToUncovered(es->pc,
                                              es->stack.back().minDistToUncoveredOnReturn);

    double invMD2U = 1. / (md2u ? md2u : 10000);
    if (type==CoveringNew) {
      double invCovNew = 0.;
      if (es->instsSinceCovNew)
        invCovNew = 1. / std::max(1, (int) es->instsSinceCovNew - 1000);
      return (invCovNew * invCovNew + invMD2U * invMD2U);
    } else {
      return invMD2U * invMD2U;
    }
  }
  }
}

void WeightedRandomSearcher::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  if (current && updateWeights &&
      std::find(removedStates.begin(), removedStates.end(), current) ==
          removedStates.end())
    states->update(current, getWeight(current));

  for (std::vector<ExecutionState *>::const_iterator it = addedStates.begin(),
                                                     ie = addedStates.end();
       it != ie; ++it) {
    ExecutionState *es = *it;
    states->insert(es, getWeight(es));
  }

  for (std::vector<ExecutionState *>::const_iterator it = removedStates.begin(),
                                                     ie = removedStates.end();
       it != ie; ++it) {
    states->remove(*it);
  }
}

bool WeightedRandomSearcher::empty() { 
  return states->empty(); 
}

///

RandomPathSearcher::RandomPathSearcher(Executor &_executor)
  : executor(_executor) {
}

RandomPathSearcher::~RandomPathSearcher() {
}

ExecutionState &RandomPathSearcher::selectState() {
  unsigned flips=0, bits=0;
  PTree::Node *n = executor.processTree->root;
  
  while (!n->data) {
    if (!n->left) {
      n = n->right;
    } else if (!n->right) {
      n = n->left;
    } else {
      if (bits==0) {
        flips = theRNG.getInt32();
        bits = 32;
      }
      --bits;
      n = (flips&(1<<bits)) ? n->left : n->right;
    }
  }

  return *n->data;
}

void
RandomPathSearcher::update(ExecutionState *current,
                           const std::vector<ExecutionState *> &addedStates,
                           const std::vector<ExecutionState *> &removedStates) {
}

bool RandomPathSearcher::empty() { 
  return executor.states.empty(); 
}

///

BumpMergingSearcher::BumpMergingSearcher(Executor &_executor, Searcher *_baseSearcher) 
  : executor(_executor),
    baseSearcher(_baseSearcher),
    mergeFunction(executor.kmodule->kleeMergeFn) {
}

BumpMergingSearcher::~BumpMergingSearcher() {
  delete baseSearcher;
}

///

Instruction *BumpMergingSearcher::getMergePoint(ExecutionState &es) {  
  if (mergeFunction) {
    Instruction *i = es.pc->inst;

    if (i->getOpcode()==Instruction::Call) {
      CallSite cs(cast<CallInst>(i));
      if (mergeFunction==cs.getCalledFunction())
        return i;
    }
  }

  return 0;
}

ExecutionState &BumpMergingSearcher::selectState() {
entry:
  // out of base states, pick one to pop
  if (baseSearcher->empty()) {
    std::map<llvm::Instruction*, ExecutionState*>::iterator it = 
      statesAtMerge.begin();
    ExecutionState *es = it->second;
    statesAtMerge.erase(it);
    ++es->pc;

    baseSearcher->addState(es);
  }

  ExecutionState &es = baseSearcher->selectState();

  if (Instruction *mp = getMergePoint(es)) {
    std::map<llvm::Instruction*, ExecutionState*>::iterator it = 
      statesAtMerge.find(mp);

    baseSearcher->removeState(&es);

    if (it==statesAtMerge.end()) {
      statesAtMerge.insert(std::make_pair(mp, &es));
    } else {
      ExecutionState *mergeWith = it->second;
      if (mergeWith->merge(es)) {
        // hack, because we are terminating the state we need to let
        // the baseSearcher know about it again
        baseSearcher->addState(&es);
        executor.terminateState(es);
      } else {
        it->second = &es; // the bump
        ++mergeWith->pc;

        baseSearcher->addState(mergeWith);
      }
    }

    goto entry;
  } else {
    return es;
  }
}

void BumpMergingSearcher::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  baseSearcher->update(current, addedStates, removedStates);
}

///

MergingSearcher::MergingSearcher(Executor &_executor, Searcher *_baseSearcher) 
  : executor(_executor),
    baseSearcher(_baseSearcher),
    mergeFunction(executor.kmodule->kleeMergeFn) {
}

MergingSearcher::~MergingSearcher() {
  delete baseSearcher;
}

///

Instruction *MergingSearcher::getMergePoint(ExecutionState &es) {
  if (mergeFunction) {
    Instruction *i = es.pc->inst;

    if (i->getOpcode()==Instruction::Call) {
      CallSite cs(cast<CallInst>(i));
      if (mergeFunction==cs.getCalledFunction())
        return i;
    }
  }

  return 0;
}

ExecutionState &MergingSearcher::selectState() {
  // FIXME: this loop is endless if baseSearcher includes RandomPathSearcher.
  // The reason is that RandomPathSearcher::removeState() does nothing...
  while (!baseSearcher->empty()) {
    ExecutionState &es = baseSearcher->selectState();
    if (getMergePoint(es)) {
      baseSearcher->removeState(&es, &es);
      statesAtMerge.insert(&es);
    } else {
      return es;
    }
  }
  
  // build map of merge point -> state list
  std::map<Instruction*, std::vector<ExecutionState*> > merges;
  for (std::set<ExecutionState*>::const_iterator it = statesAtMerge.begin(),
         ie = statesAtMerge.end(); it != ie; ++it) {
    ExecutionState &state = **it;
    Instruction *mp = getMergePoint(state);
    
    merges[mp].push_back(&state);
  }
  
  if (DebugLogMerge)
    llvm::errs() << "-- all at merge --\n";
  for (std::map<Instruction*, std::vector<ExecutionState*> >::iterator
         it = merges.begin(), ie = merges.end(); it != ie; ++it) {
    if (DebugLogMerge) {
      llvm::errs() << "\tmerge: " << it->first << " [";
      for (std::vector<ExecutionState*>::iterator it2 = it->second.begin(),
             ie2 = it->second.end(); it2 != ie2; ++it2) {
        ExecutionState *state = *it2;
        llvm::errs() << state << ", ";
      }
      llvm::errs() << "]\n";
    }

    // merge states
    std::set<ExecutionState*> toMerge(it->second.begin(), it->second.end());
    while (!toMerge.empty()) {
      ExecutionState *base = *toMerge.begin();
      toMerge.erase(toMerge.begin());
      
      std::set<ExecutionState*> toErase;
      for (std::set<ExecutionState*>::iterator it = toMerge.begin(),
             ie = toMerge.end(); it != ie; ++it) {
        ExecutionState *mergeWith = *it;
        
        if (base->merge(*mergeWith)) {
          toErase.insert(mergeWith);
        }
      }
      if (DebugLogMerge && !toErase.empty()) {
        llvm::errs() << "\t\tmerged: " << base << " with [";
        for (std::set<ExecutionState*>::iterator it = toErase.begin(),
               ie = toErase.end(); it != ie; ++it) {
          if (it!=toErase.begin()) llvm::errs() << ", ";
          llvm::errs() << *it;
        }
        llvm::errs() << "]\n";
      }
      for (std::set<ExecutionState*>::iterator it = toErase.begin(),
             ie = toErase.end(); it != ie; ++it) {
        std::set<ExecutionState*>::iterator it2 = toMerge.find(*it);
        assert(it2!=toMerge.end());
        executor.terminateState(**it);
        toMerge.erase(it2);
      }

      // step past merge and toss base back in pool
      statesAtMerge.erase(statesAtMerge.find(base));
      ++base->pc;
      baseSearcher->addState(base);
    }  
  }
  
  if (DebugLogMerge)
    llvm::errs() << "-- merge complete, continuing --\n";
  
  return selectState();
}

void
MergingSearcher::update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) {
  if (!removedStates.empty()) {
    std::vector<ExecutionState *> alt = removedStates;
    for (std::vector<ExecutionState *>::const_iterator
             it = removedStates.begin(),
             ie = removedStates.end();
         it != ie; ++it) {
      ExecutionState *es = *it;
      std::set<ExecutionState*>::const_iterator it2 = statesAtMerge.find(es);
      if (it2 != statesAtMerge.end()) {
        statesAtMerge.erase(it2);
        alt.erase(std::remove(alt.begin(), alt.end(), es), alt.end());
      }
    }    
    baseSearcher->update(current, addedStates, alt);
  } else {
    baseSearcher->update(current, addedStates, removedStates);
  }
}

///

BatchingSearcher::BatchingSearcher(Searcher *_baseSearcher,
                                   double _timeBudget,
                                   unsigned _instructionBudget) 
  : baseSearcher(_baseSearcher),
    timeBudget(_timeBudget),
    instructionBudget(_instructionBudget),
    lastState(0) {
  
}

BatchingSearcher::~BatchingSearcher() {
  delete baseSearcher;
}

ExecutionState &BatchingSearcher::selectState() {
  if (!lastState || 
      (util::getWallTime()-lastStartTime)>timeBudget ||
      (stats::instructions-lastStartInstructions)>instructionBudget) {
    if (lastState) {
      double delta = util::getWallTime()-lastStartTime;
      if (delta>timeBudget*1.1) {
        klee_message("KLEE: increased time budget from %f to %f\n", timeBudget,
                     delta);
        timeBudget = delta;
      }
    }
    lastState = &baseSearcher->selectState();
    lastStartTime = util::getWallTime();
    lastStartInstructions = stats::instructions;
    return *lastState;
  } else {
    return *lastState;
  }
}

void
BatchingSearcher::update(ExecutionState *current,
                         const std::vector<ExecutionState *> &addedStates,
                         const std::vector<ExecutionState *> &removedStates) {
  if (std::find(removedStates.begin(), removedStates.end(), lastState) !=
      removedStates.end())
    lastState = 0;
  baseSearcher->update(current, addedStates, removedStates);
}

/***/

IterativeDeepeningTimeSearcher::IterativeDeepeningTimeSearcher(Searcher *_baseSearcher)
  : baseSearcher(_baseSearcher),
    time(1.) {
}

IterativeDeepeningTimeSearcher::~IterativeDeepeningTimeSearcher() {
  delete baseSearcher;
}

ExecutionState &IterativeDeepeningTimeSearcher::selectState() {
  ExecutionState &res = baseSearcher->selectState();
  startTime = util::getWallTime();
  return res;
}

void IterativeDeepeningTimeSearcher::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  double elapsed = util::getWallTime() - startTime;

  if (!removedStates.empty()) {
    std::vector<ExecutionState *> alt = removedStates;
    for (std::vector<ExecutionState *>::const_iterator
             it = removedStates.begin(),
             ie = removedStates.end();
         it != ie; ++it) {
      ExecutionState *es = *it;
      std::set<ExecutionState*>::const_iterator it2 = pausedStates.find(es);
      if (it2 != pausedStates.end()) {
        pausedStates.erase(it2);
        alt.erase(std::remove(alt.begin(), alt.end(), es), alt.end());
      }
    }    
    baseSearcher->update(current, addedStates, alt);
  } else {
    baseSearcher->update(current, addedStates, removedStates);
  }

  if (current &&
      std::find(removedStates.begin(), removedStates.end(), current) ==
          removedStates.end() &&
      elapsed > time) {
    pausedStates.insert(current);
    baseSearcher->removeState(current);
  }

  if (baseSearcher->empty()) {
    time *= 2;
    klee_message("KLEE: increased time budget to %f\n", time);
    std::vector<ExecutionState *> ps(pausedStates.begin(), pausedStates.end());
    baseSearcher->update(0, ps, std::vector<ExecutionState *>());
    pausedStates.clear();
  }
}

/***/

InterleavedSearcher::InterleavedSearcher(const std::vector<Searcher*> &_searchers)
  : searchers(_searchers),
    index(1) {
}

InterleavedSearcher::~InterleavedSearcher() {
  for (std::vector<Searcher*>::const_iterator it = searchers.begin(),
         ie = searchers.end(); it != ie; ++it)
    delete *it;
}

ExecutionState &InterleavedSearcher::selectState() {
  Searcher *s = searchers[--index];
  if (index==0) index = searchers.size();
  return s->selectState();
}

void InterleavedSearcher::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  for (std::vector<Searcher*>::const_iterator it = searchers.begin(),
         ie = searchers.end(); it != ie; ++it)
    (*it)->update(current, addedStates, removedStates);
}

///

CastanSearcher::CastanSearcher(const llvm::Module *module) {
  klee_message("Generating global cost map for directed search.");

  // ICFG
  std::map<const llvm::Instruction *, std::set<const llvm::Instruction *>>
      predecessors, successors;
  std::map<const llvm::Function *, std::set<const llvm::Instruction *>> callers;
  // Path from instruction to target without order.
  std::map<const llvm::Instruction *,
           std::map<const llvm::Instruction *, int>> paths;


  // Generate ICFG and initialize cost map.
  klee_message("  Computing ICFG.");
  for (auto &fn : *module) {
    for (auto &bb : fn) {
      const llvm::Instruction *prevInst = NULL;
      for (auto &inst : bb) {
        if (prevInst) {
          predecessors[&inst].insert(prevInst);
          successors[prevInst].insert(&inst);
        }
        prevInst = &inst;

        if (const llvm::CallInst *ci = dyn_cast<llvm::CallInst>(&inst)) {
          if (ci->getCalledFunction()) {
            callers[ci->getCalledFunction()].insert(ci);
          }
        }
      }
      for (unsigned i = 0; i < bb.getTerminator()->getNumSuccessors(); i++) {
        predecessors[&bb.getTerminator()->getSuccessor(i)->front()].insert(
            bb.getTerminator());
        successors[bb.getTerminator()].insert(
            &bb.getTerminator()->getSuccessor(i)->front());
      }
    }
  }

  std::set<const llvm::Instruction *> worklist;

  // Initialize cost map.
  klee_message("  Initializing cost map.");
  for (auto &fn : *module) {
    for (auto &bb : fn) {
      for (auto &inst : bb) {
        if (successors[&inst].empty()) {
          costs[&inst].first = false;
          costs[&inst].second = 1;
          paths[&inst][&inst] = 1;

          // Propagate changes to predecessors.
          worklist.insert(predecessors[&inst].begin(),
                          predecessors[&inst].end());
          // Check if entry instruction and propagate to callers.
          if (&inst ==
              &inst.getParent()->getParent()->getEntryBlock().front()) {
            worklist.insert(callers[inst.getParent()->getParent()].begin(),
                            callers[inst.getParent()->getParent()].end());
          }
        }
      }
    }
  }
  llvm::Function *loopAnnotation = module->getFunction("castan_loop");
  assert(loopAnnotation);
  for (auto inst : callers[loopAnnotation]) {
    costs[inst].first = true;
    costs[inst].second = 1;
    paths[inst][inst] = 1;

    // Propagate changes to predecessors.
    worklist.insert(predecessors[inst].begin(), predecessors[inst].end());
    // Check if entry instruction and propagate to callers.
    if (inst == &(inst->getParent()->getParent()->getEntryBlock().front())) {
      worklist.insert(callers[inst->getParent()->getParent()].begin(),
                      callers[inst->getParent()->getParent()].end());
    }
  }

  klee_message("  Computing cost map.");
  while (!worklist.empty()) {
    auto inst = *worklist.begin();
    worklist.erase(inst);
    // Flag if the cost changed and should be propagates back.
    bool changed = false;

    const llvm::CallInst *ci = dyn_cast<llvm::CallInst>(inst);
    if (ci && ci->getCalledFunction() == loopAnnotation) {
    } else if (ci && ci->getCalledFunction() &&
               !ci->getCalledFunction()->empty()) {
      assert(paths[&ci->getCalledFunction()->front().front()].count(inst) == 0);

      std::map<const llvm::Instruction *, int> path;
      std::pair<bool, long> cost;
      // Check if called function is on direct path.
      if (costs[&ci->getCalledFunction()->front().front()].first) {
        cost.first = true;
        cost.second = costs[&ci->getCalledFunction()->front().front()].second + 1;
        path = paths[&ci->getCalledFunction()->front().front()];
        path[inst]++;
      } else {
        assert(successors[inst].size() == 1);
        auto s = *successors[inst].begin();
        cost.first = costs[s].first;
        if (!(paths[&ci->getCalledFunction()->front().front()].empty() ||
              paths[s].empty())) {
          path[inst]++;
          for (auto it : paths[&ci->getCalledFunction()->front().front()]) {
            path[it.first] += it.second;
          }
          for (auto it : paths[s]) {
            path[it.first] += it.second;
          }
          cost.second = 1 +
              costs[&ci->getCalledFunction()->front().front()].second +
              costs[s].second;
          successorCosts[ci] = costs[s].second;
        }
      }
      if (cost != costs[inst]) {
        paths[inst] = path;
        costs[inst] = cost;
        changed = true;
      }
    } else {
      long cost =
          (isa<llvm::LoadInst>(inst) || isa<llvm::StoreInst>(inst)) ? 4 : 1;
      // Look at successors within function.
      for (auto s : successors[inst]) {
        if (paths[s][inst] <= 1) {
          if (costs[inst].first) {
            if (costs[s].first && costs[s].second + cost > costs[inst].second) {
              costs[inst].second = costs[s].second + cost;
              paths[inst] = paths[s];
              paths[inst][inst]++;
              changed = true;
            }
          } else {
            if (costs[s].first || costs[s].second + cost > costs[inst].second) {
              costs[inst].first = costs[s].first;
              costs[inst].second = costs[s].second + cost;
              paths[inst] = paths[s];
              paths[inst][inst]++;
              changed = true;
            }
          }
        }
      }
    }

    if (changed) {
      // Add predecessors to worklist.
      if (inst == &inst->getParent()->getParent()->front().front()) {
        // Predecessors from before function call.
        worklist.insert(callers[inst->getParent()->getParent()].begin(),
                        callers[inst->getParent()->getParent()].end());
      } else {
        worklist.insert(predecessors[inst].begin(), predecessors[inst].end());
      }
    }
  }

//   klee_message("  Dumping ICFG to ./icfg/.");
//   auto result = mkdir("icfg", 0755);
//   assert(result == 0 || errno == EEXIST);
// 
//   std::ofstream makefile("icfg/Makefile");
//   assert(makefile.good());
//   makefile << "%.pdf: %.dot" << std::endl;
//   makefile << "\tdot -Tpdf -o $@ $<" << std::endl << std::endl;
// 
//   makefile << "%.svg: %.dot" << std::endl;
//   makefile << "\tdot -Tsvg -o $@ $<" << std::endl << std::endl;
// 
//   makefile << "%.cmapx: %.dot" << std::endl;
//   makefile << "\tdot -Tcmapx -o $@ $<" << std::endl << std::endl;
// 
//   makefile << "%.html: %.cmapx" << std::endl;
//   makefile
//       << "\techo \"<html><img src='$(@:.html=.svg)' usemap='#CFG' />\" > $@"
//       << std::endl;
//   makefile << "\tcat $< >> $@" << std::endl;
//   makefile << "\techo '</html>' >> $@" << std::endl << std::endl;
// 
//   makefile << "%.clean:" << std::endl;
//   makefile << "\trm -f $(@:.clean=.html) $(@:.clean=.svg) $(@:.clean=.cmapx)"
//             << std::endl << std::endl;
// 
//   makefile << "default: all" << std::endl;
// 
//   for (auto &fn : *module) {
//     klee::klee_message("Generating %s.dot", fn.getName().str().c_str());
// 
//     std::ofstream dotFile("icfg/" + fn.getName().str() + ".dot");
//     assert(dotFile.good());
//     // Generate CFG DOT file.
//     dotFile << "digraph CFG {" << std::endl;
// 
//     std::string signature;
//     llvm::raw_string_ostream ss(signature);
//     fn.getFunctionType()->print(ss);
//     ss.flush();
//     std::replace(signature.begin(), signature.end(), '\"', '\'');
//     dotFile << "	label = \"" << fn.getName().str() << ": " << signature
//             << "\"" << std::endl;
//     dotFile << "	labelloc = \"t\"" << std::endl;
// 
//     unsigned long entryRef = fn.empty()
//                                   ? (unsigned long)&fn
//                                   : (unsigned
//                                   long)&fn.getEntryBlock().front();
//     // Add edges to definite callers.
//     dotFile << "  edge [color = \"blue\"];" << std::endl;
//     for (auto caller : callers[&fn]) {
//       dotFile << "  n" << ((unsigned long)caller) << " -> n" << entryRef <<
//       ";"
//               << std::endl;
//       dotFile << "  n" << ((unsigned long)caller) << " [label = \""
//               << caller->getParent()->getParent()->getName().str()
//               << "\" shape = \"invhouse\" href=\""
//               << caller->getParent()->getParent()->getName().str() <<
//               ".html\"]"
//               << std::endl;
//     }
// 
//     // Add all instructions in the function.
//     for (auto &bb : fn) {
//       for (auto &inst : bb) {
//         std::stringstream attributes;
//         // Annotate entry / exit points.
//         if (successors[&inst].empty()) {
//           attributes << "shape = \"doublecircle\"";
//         } else if (&inst ==
//                     &(inst.getParent()->getParent()->getEntryBlock().front()))
//                     {
//           attributes << "shape = \"box\"";
//         } else {
//           attributes << "shape = \"circle\"";
//         }
//         // Annotate source line and instruction print-out.
//         if (costs[&inst].second == LONG_MAX) {
//           attributes << " label = \"--\"";
//         } else {
//           attributes << " label = \"" << costs[&inst].second << "\"";
//         }
//         attributes << " tooltip = \"" << inst.getOpcodeName() << "\"";
//         // Annotate utility color.
//         attributes << " style=\"filled\" fillcolor = \""
//                     << (costs[&inst].first ? "lightgreen" : "lightblue") <<
//                     "\"";
//         dotFile << "	n" << ((unsigned long)&inst) << " [" <<
//         attributes.str()
//                 << "];" << std::endl;
//         // Add edges to successors.
//         dotFile << "	edge [color = \"black\"];" << std::endl;
//         for (auto successor : successors[&inst]) {
//           dotFile << "	n" << ((unsigned long)&inst) << " -> n"
//                   << ((unsigned long)successor) << ";" << std::endl;
//         }
//         // Add edges to definite callees.
//         dotFile << "	edge [color = \"blue\"];" << std::endl;
//         if (const llvm::CallInst *ci = dyn_cast<const
//         llvm::CallInst>(&inst)) {
//           if (ci->getCalledFunction()) {
//             dotFile << "	n" << ((unsigned long)&inst) << " -> n"
//                     << ((unsigned long)ci->getCalledFunction()) << ";"
//                     << std::endl;
//             dotFile << "	n" << ((unsigned long)ci->getCalledFunction())
//                     << " [label = \""
//                     << ci->getCalledFunction()->getName().str()
//                     << "\" shape = \"folder\" href=\""
//                     << ci->getCalledFunction()->getName().str() <<
//                     ".html\"]"
//                     << std::endl;
//           } else {
//             dotFile
//                 << "	IndirectFunction [label = \"*\" shape = \"folder\"]"
//                 << std::endl;
//             dotFile << "	n" << ((unsigned long)&inst)
//                     << " -> IndirectFunction;" << std::endl;
//           }
//         }
//       }
//     }
//     dotFile << "}" << std::endl;
// 
//     makefile << "all: " << fn.getName().str() << std::endl;
//     makefile << fn.getName().str() << ": " << fn.getName().str() << ".html "
//               << fn.getName().str() << ".svg" << std::endl;
//     makefile << "clean: " << fn.getName().str() << ".clean" << std::endl;
//   }
//   exit(0);
}

long CastanSearcher::getPriority(ExecutionState *state) {
  if (state->cacheModel->getNumIterations() == 0) {
    return LONG_MAX;
  }

  assert(costs.count(state->pc->inst));
  long result = state->cacheModel->getTotalCycles();

  result += costs[state->pc->inst].second;
  if (costs[state->pc->inst].first) {
    return result / state->cacheModel->getNumIterations();
  }

  for (klee::ExecutionState::stack_ty::const_reverse_iterator
           it = state->stack.rbegin(),
           ie = state->stack.rend();
       it != ie && it->caller; it++) {
    result += successorCosts[it->caller->inst];
    if (costs[it->caller->inst].first) {
      return result / state->cacheModel->getNumIterations();
    }
  }
  return result / state->cacheModel->getNumIterations();
}

ExecutionState &CastanSearcher::selectState() {
  return *states.rbegin()->second;
}

void CastanSearcher::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  if (current) {
    bool found = false;
    for (auto it : states) {
      if (it.second == current) {
        states.erase(it);
        states.insert(std::make_pair(getPriority(current), current));
        found = true;
        break;
      }
    }
    assert(found && "invalid current state");
  }
  for (auto it : addedStates) {
    states.insert(std::make_pair(getPriority(it), it));
  }
  for (auto rit : removedStates) {
    bool found = false;
    for (auto sit : states) {
      if (rit == sit.second) {
        states.erase(sit);
        found = true;
        break;
      }
    }
    assert(found && "invalid state removed");
  }
}
