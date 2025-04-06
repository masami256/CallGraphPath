#pragma once

#include <llvm/IR/Module.h>
#include <set>

typedef std::vector< std::pair<llvm::Module*, llvm::StringRef> > ModuleList;
// Mapping module to its file name.
typedef std::unordered_map<llvm::Module*, llvm::StringRef> ModuleNameMap;
// The set of strings.
typedef std::set<std::string> StrSet;
typedef llvm::SmallPtrSet<llvm::Function*, 8> FuncSet;

using namespace llvm;
struct GlobalContext {
    GlobalContext() {
        // Initialize the context
    }

    // Modules.
	ModuleList Modules;
};
