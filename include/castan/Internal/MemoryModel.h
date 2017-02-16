#include "llvm/Support/CommandLine.h"

#define MEMORY_MODEL_PREFIX "memory_model_"

#define MEMORY_MODEL_INIT_SUFFIX "_init"
#define MEMORY_MODEL_EXEC_SUFFIX "_exec"
#define MEMORY_MODEL_LOAD_SUFFIX "_load"
#define MEMORY_MODEL_STORE_SUFFIX "_store"
#define MEMORY_MODEL_DONE_SUFFIX "_done"

#define MEMORY_MODEL_PRIORITY_SUFFIX "_priority"

extern llvm::cl::opt<std::string> MemModel;
