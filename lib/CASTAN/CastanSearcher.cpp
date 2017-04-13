#include "../Core/Searcher.h"

#include "castan/Internal/CacheModel.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include <chrono>
#include <sys/stat.h>

namespace castan {
CastanSearcher::CastanSearcher(const llvm::Module *module) {
  klee::klee_message("Generating global cost map for directed search.");

  // ICFG
  std::map<const llvm::Instruction *, std::set<const llvm::Instruction *>>
      predecessors, successors;
  std::map<const llvm::Function *, std::set<const llvm::Instruction *>> callers;
  // Path from instruction to target without order.
  std::map<const llvm::Instruction *, std::map<const llvm::Instruction *, int>>
      paths;

  // Generate ICFG and initialize cost map.
  klee::klee_message("  Computing ICFG.");
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
  klee::klee_message("  Initializing cost map.");
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

  klee::klee_message("  Computing cost map.");
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
        cost.second =
            costs[&ci->getCalledFunction()->front().front()].second + 1;
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
          cost.second =
              1 + costs[&ci->getCalledFunction()->front().front()].second +
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
  //       << "\techo \"<html><img src='$(@:.html=.svg)' usemap='#CFG' />\" >
  //       $@"
  //       << std::endl;
  //   makefile << "\tcat $< >> $@" << std::endl;
  //   makefile << "\techo '</html>' >> $@" << std::endl << std::endl;
  //
  //   makefile << "%.clean:" << std::endl;
  //   makefile << "\trm -f $(@:.clean=.html) $(@:.clean=.svg)
  //   $(@:.clean=.cmapx)"
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
  //                 << "	IndirectFunction [label = \"*\" shape =
  //                 \"folder\"]"
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
  //     makefile << fn.getName().str() << ": " << fn.getName().str() << ".html
  //     "
  //               << fn.getName().str() << ".svg" << std::endl;
  //     makefile << "clean: " << fn.getName().str() << ".clean" << std::endl;
  //   }
  //   exit(0);
}

std::vector<long> CastanSearcher::getPriority(klee::ExecutionState *state) {
  if (state->cacheModel->getNumIterations() == 0) {
    return {LONG_MAX, 0, 0};
  }

  assert(costs.count(state->pc->inst));
  long result = state->cacheModel->getTotalCycles();

  result += costs[state->pc->inst].second;
  if (costs[state->pc->inst].first) {
    return {result / state->cacheModel->getNumIterations(),
            state->cacheModel->getTotalCycles()};
  }

  for (klee::ExecutionState::stack_ty::const_reverse_iterator
           it = state->stack.rbegin(),
           ie = state->stack.rend();
       it != ie && it->caller; it++) {
    result += successorCosts[it->caller->inst];
    if (costs[it->caller->inst].first) {
      return {result / state->cacheModel->getNumIterations(),
              state->cacheModel->getTotalCycles()};
    }
  }
  return {result / state->cacheModel->getNumIterations(),
          state->cacheModel->getTotalCycles()};
}

klee::ExecutionState &CastanSearcher::selectState() {
  static std::chrono::time_point<std::chrono::system_clock> lastReportTime =
      std::chrono::system_clock::now();

  if (std::chrono::duration_cast<std::chrono::milliseconds>(
          (std::chrono::system_clock::now() - lastReportTime))
          .count() >= 1000) {
    std::stringstream ss;
    for (auto pit : states.rbegin()->first) {
      ss << ", " << pit;
    }

    klee::klee_message("Processing %ld states. Current state with %d "
                       "iterations and priority [%s]:",
                       states.size(),
                       states.rbegin()->second->cacheModel->getNumIterations(),
                       ss.str().substr(2).c_str());
    states.rbegin()->second->dumpStack(llvm::errs());

    klee::klee_message("States:");
    for (auto sit : states) {
      std::stringstream ss;
      for (auto pit : sit.first) {
        ss << ", " << pit;
      }

      klee::klee_message(
          "  State %p with %d "
          "iterations and priority [%s] at %s:%d",
          (void *)sit.second, sit.second->cacheModel->getNumIterations(),
          ss.str().substr(2).c_str(), sit.second->pc->info->file.c_str(),
          sit.second->pc->info->line);
    }

    lastReportTime = std::chrono::system_clock::now();
  }

  return *states.rbegin()->second;
}

void CastanSearcher::update(
    klee::ExecutionState *current,
    const std::vector<klee::ExecutionState *> &addedStates,
    const std::vector<klee::ExecutionState *> &removedStates) {
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
}