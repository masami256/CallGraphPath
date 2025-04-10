#pragma once
#include "Analyzer.h"
#include <llvm/IR/Module.h>

#include <map>
#include <vector>

using namespace llvm;

// ModuleFunctionMap is a map where:
// - The key is the module name (string) (e.g., "my_module.bc").
// - The value is another map that associates function names (string) (e.g., "main", "foo") 
//   with a vector of tuples.
// Each tuple in the vector represents the function prototype and contains:
// - std::string: the return type of the function (e.g., "i32" for integer, "void" for functions with no return).
// - std::vector<std::string>: a list of argument types for the function (e.g., ["i32", "i8*"] for a function taking an integer and a pointer).
// - std::string: the source line where the function is defined (e.g., "10" for line 10 in the source code).
// Example:
// Given the following function prototype in a module:

// void bar(i32 x, i8* ptr) defined on line 10 of "my_module.bc":
// {
//   "my_module.bc" -> {
//     "bar" -> {
//       ("void", ["i32", "i8*"], "10")
//     }
//   }
// }
// This means that the module "my_module.bc" has a function "bar" with a return type of "void",
// two arguments: an integer ("i32") and a pointer to a character ("i8*"), and it is defined on line 10 of the source code.
using ModuleFunctionMap = std::map<std::string, 
    std::map<std::string, 
    std::vector<std::tuple<std::string, std::vector<std::string>, 
    std::string>>>>;


// FunctionPointerSettingInfo: Stores function pointer setting information with an offset
struct FunctionPointerSettingInfo {
    std::string ModName;          // Module name
    std::string VarName;          // The Variable name
    std::string SetterName;       // The name of the function or global variable where the pointer is set
    std::string StructTypeName;   // Struct type name for struct function pointers
    std::string FuncName;         // The function being pointed to
    unsigned Line;                // The line number where the pointer is set
    unsigned Offset;              // The offset of the function pointer in the struct
};

// FunctionPointerSettings: Stores all function pointer settings for a module.
using FunctionPointerSettings = std::map<std::string, std::vector<FunctionPointerSettingInfo>>;

// Structure to store information about function pointer calls
struct FunctionPointerCallInfo {
    std::string ModName;         // Module name where the function call occurs
    std::string CallerFuncName;  // Name of the calling function
    std::string CalleeFuncName;  // Name of the called function
    unsigned Line;               // Line number where the function pointer is called
    unsigned ArgIndex;           // Index number of argument parameter
};

// Map to store FunctionPointerCallInfo, with key as "ModuleName:LineNumber"
using FunctionPointerCallMap = std::map<std::string, std::vector<FunctionPointerCallInfo>>;

// Structure for function pointer usage (from the callee site, e.g., bar)
struct FunctionPointerUseInfo {
    std::string ModName;
    std::string CallerFuncName; // e.g., bar
    std::string CalleeFuncName; // typically "indirect"
    unsigned Line;
    unsigned ArgIndex;
};

using FunctionPointerUseMap = std::map<std::string, std::vector<FunctionPointerUseInfo>>;


// Call-edge
// Call-edge structure
struct CallEdgeInfo {
    std::string CallerModule;     // Module name where the call occurs
    std::string CallerFunction;   // Function from which the call is made
    std::string CalleeFunction;   // Function being called (can be "indirect")
    unsigned Line;                // Source line of the call
    bool IsIndirect;              // True if the call is indirect
    std::string VarName;          // Name of the variable used in the call (only for indirect calls)
    unsigned Offset;              // Offset within struct if applicable (added for matching)
};


// Callgraph
using ModuleCallGraph = std::map<std::string, std::vector<CallEdgeInfo>>;


class CallGraphPass {
    private:
        ModuleFunctionMap ModuleFunctionMap;
        FunctionPointerSettings FunctionPointerSettings;
        // Record the function pointer setting along with the offset in the struct
        std::set<std::tuple<std::string, std::string, unsigned, unsigned>> ProcessedSettings;

        FunctionPointerCallMap FunctionPointerCalls;
        FunctionPointerUseMap FunctionPointerUses;
        ModuleCallGraph CallGraph;

        void CollectFunctionProtoTypes(Module *M);
        void CollectStaticFunctionPointerAssignments(Module *M);
        void CollectCallingAddressTakenFunction(Module *M);
        void CollectDynamicFunctionPointerAssignments(Module *M);
        void CollectFunctionPointerArgumentPassing(Module *M);
        void AnalyzeDirectCalls(Module *M);
        void AnalyzeIndirectCalls();
        void ResolveIndirectCalls();
        void AnalyzeStaticFPCallSites();
        void AnalyzeStaticGlobalFPCalls();

        void RecordFunctionPointerSetting(
            const std::string &ModName,
            const std::string &SetterName,
            const std::string &StructTypeName,
            const std::string &FuncName,
            unsigned Line,
            unsigned Offset);

        void RecordFunctionPointerCall(
            const std::string &ModName, 
            const std::string &CallerFuncName, 
            const std::string &CalleeFuncName, 
            unsigned Line,
            unsigned ArgIndex);

        void RecordFunctionPointerUse(
            const std::string &ModName,
            const std::string &CallerFuncName,
            const std::string &CalleeFuncName,
            unsigned Line,
            unsigned ArgIndex);

        void RecordCallGraphEdge(
            const std::string &ModName,
            const std::string &CallerFunc,
            const std::string &CalleeFunc,
            unsigned Line,
            bool IsIndirect,
            const std::string &VarName = "",
            unsigned Offset = 0);

        unsigned getLineNumber(const Instruction *I) {
            if (const DebugLoc &DL = I->getDebugLoc()) {
                return DL.getLine();
            }
            return 0;
        }

            
    protected:
        const char * ID;
    public:
        CallGraphPass(const char *ID_)
        : ID(ID_) { }
        
        void run(ModuleList &modules);
        bool CollectInformation(Module *M);
        bool IdentifyTargets(llvm::Module *M);
};
