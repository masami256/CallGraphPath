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
	ModuleNameMap ModuleMaps;
};

class IterativeModulePass {
    protected:
        GlobalContext *Ctx;
        const char * ID;
    public:
        IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
            : Ctx(Ctx_), ID(ID_) { }
    
        // Run on each module before iterative pass.
        virtual bool CollectInformation(Module *M)
            { return true; }
    
        // Run on each module after iterative pass.
        //virtual bool doFinalization(llvm::Module *M)
        //	{ return true; }
    
        // Iterative pass.
        virtual bool IdentifyTargets(llvm::Module *M)
            { return false; }
    
        virtual void run(ModuleList &modules);
};