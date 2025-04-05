#pragma once
#include "Analyzer.h"
#include <llvm/IR/Module.h>

#include <map>
#include <vector>

using namespace llvm;

// Map to store: module name -> list of function prototype strings
typedef std::map<std::string, std::vector<std::string>> ModuleFunctionMap;

typedef std::map<std::string, std::map<std::string, std::vector<std::string>>> StaticFunctionPointerMap;

class CallGraphPass : public IterativeModulePass {
    private:
        void CollectFunctionProtoTypes(Module *M);
        void CollectStaticFunctionPointerInit(Module *M);
        void RecordStaticFuncPtrInit(StringRef StructTypeName, StringRef VarName, unsigned Index, StringRef FuncName);
    public:
        CallGraphPass(GlobalContext *Ctx_)
            : IterativeModulePass(Ctx_, "CallGraph") { }
        
        virtual bool CollectInformation(Module *M);
        //virtual bool doFinalization(llvm::Module *);
        virtual bool IdentifyTargets(llvm::Module *M);
};
