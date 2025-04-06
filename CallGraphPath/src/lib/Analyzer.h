#pragma once

#include <llvm/IR/Module.h>
#include <set>

typedef std::vector< std::pair<llvm::Module*, llvm::StringRef> > ModuleList;

