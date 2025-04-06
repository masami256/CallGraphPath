#pragma once
#include "Analyzer.h"
#include <llvm/IR/Module.h>

#include <map>
#include <vector>

using namespace llvm;

// Map to store: module name -> list of function prototype strings
typedef std::map<std::string, std::vector<std::string>> ModuleFunctionMap;

typedef std::map<std::string, std::map<std::string, std::vector<std::string>>> StaticFunctionPointerMap;

typedef std::map<
    std::string, // Module Name（Module->getName()）
    std::map<
        std::string,               // Function name which the function pointer is assigned to
        std::vector<std::string>  // String of "Callee:AssignedTo:Line"
    >
> DynamicFunctionPointerMap;

typedef std::map<
    std::string, // Module Name（Module->getName()）
    std::map<
        std::string,             // Callee function name 
        std::vector<std::string> // Pointer to function:Line number:Callee function:Line number
    >
> FunctionPtrArgMap;

class CallGraphPass : public IterativeModulePass {
    private:
        ModuleFunctionMap ModuleFuncMap;
        StaticFunctionPointerMap StaticFPMap;
        DynamicFunctionPointerMap DynamicFPMap;
        FunctionPtrArgMap FuncPtrArgMap;

        void CollectFunctionProtoTypes(Module *M);
        void CollectStaticFunctionPointerInit(Module *M);
        void CollectDynamicFunctionPointerInit(Module *M);
        void CollectGlobalFunctionPointerInit(Module *M);
        void CollectFunctionPointerArguments(Module *M);

        void RecordStaticFuncPtrInit(StringRef StructTypeName, StringRef VarName, unsigned Index, StringRef FuncName, unsigned);
        void RecordDynamicFuncPtrAssignment(StringRef ModuleName, StringRef InFunction,
            StringRef TargetFunc, StringRef AssignedTo, unsigned LineNumber);
        void RecordFuncPtrArgument(StringRef ModuleName, StringRef CallerFunc,
            StringRef PassedFunc, StringRef CalleeFunc,
            unsigned ArgIndex, unsigned LineNumber);

    public:
        CallGraphPass(GlobalContext *Ctx_)
            : IterativeModulePass(Ctx_, "CallGraph") { }
        
        virtual bool CollectInformation(Module *M);
        //virtual bool doFinalization(llvm::Module *);
        virtual bool IdentifyTargets(llvm::Module *M);
};
