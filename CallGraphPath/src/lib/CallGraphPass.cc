#include "CallGraphPass.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/User.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <list>

#include "Utils.h"

using namespace llvm;



void CallGraphPass::run(ModuleList &modules) {
    std::cout << "Running pass: " << ID << std::endl;

    ModuleList::iterator i, e;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
        Module *M = i->first;
        std::string ModuleName = i->second.str();

        std::cout << "Processing module: " << ModuleName << std::endl;

        if (!CollectInformation(M)) {
            std::cerr << "Error collecting information for module: " << ModuleName << std::endl;
            continue;
        }

        if (!IdentifyTargets(M)) {
            std::cerr << "Error identifying targets for module: " << ModuleName << std::endl;
            continue;
        }
        // Print the collected information
	}

    std::cout << "Pass completed: " << ID << std::endl;
}

bool CallGraphPass::CollectInformation(Module *M) {
    std::string ModName = M->getName().str();
    errs() << "Collecting information from module: " << ModName << "\n";

    CollectFunctionProtoTypes(M);
    CollectStaticFunctionPointerAssignments(M);
    CollectFunctionPointerArgumentPassing(M);
    CollectCallingAddressTakenFunction(M);
    CollectDynamicFunctionPointerAssignments(M);
    CollectDirectCalls(M);

    PrintModuleFunctionMap(ModuleFunctionMap, M->getName().str());
    PrintFunctionPointerSettings(FunctionPointerSettings);
    PrintFunctionPointerCallMap(FunctionPointerCalls);
    PrintFunctionPointerUseMap(FunctionPointerUses);

    return true;
}

bool CallGraphPass::IdentifyTargets(Module *M) {
    std::string ModName = M->getName().str();
    errs() << "Identifying targets in module: " << ModName << "\n";

    AnalyzeIndirectCalls();
    ResolveIndirectCalls();
    AnalyzeStaticFPCallSites();
    AnalyzeStaticGlobalFPCalls();

    PrintCallGraph(CallGraph);

    return true;
}


void CallGraphPass::CollectFunctionProtoTypes(Module *M) {
    std::string ModName = M->getName().str();  // Get the module name
    std::map<std::string, std::vector<std::tuple<std::string, std::vector<std::string>, std::string>>> FuncProtoTypes;

    // Iterate over all functions in the module
    for (Function &F : M->functions()) {
        // Skip function declarations (functions without a body)
        if (F.isDeclaration()) continue;

        // Get the function name
        std::string FuncName = F.getName().str();
        std::vector<std::string> ArgTypes;
        
        // Get the return type
        std::string ReturnType;
        llvm::raw_string_ostream ReturnTypeStream(ReturnType);  // Use raw_string_ostream properly
        F.getReturnType()->print(ReturnTypeStream);  // Print return type to the stream

        // Get the argument types
        for (unsigned i = 0; i < F.arg_size(); ++i) {
            std::string ArgType;
            llvm::raw_string_ostream ArgTypeStream(ArgType);  // Create a stream for each argument type
            F.getFunctionType()->getParamType(i)->print(ArgTypeStream);  // Print argument type to the stream
            ArgTypes.push_back(ArgType);
        }

        // Get the line number in the source code
        unsigned Line = 0;
        if (DISubprogram *SP = F.getSubprogram()) {
            Line = SP->getLine();
        }

        // Add the function prototype to the module's map
        FuncProtoTypes[FuncName].push_back(std::make_tuple(ReturnType, ArgTypes, std::to_string(Line)));
    }

    // Store the function prototypes under the module name
    ModuleFunctionMap[ModName] = std::move(FuncProtoTypes);
}

void CallGraphPass::CollectStaticFunctionPointerAssignments(Module *M) {
    std::string ModName = M->getName().str();

    for (GlobalVariable &GV : M->globals()) {
        // Skip if the global has no initializer
        if (!GV.hasInitializer()) continue;

        Constant *Init = GV.getInitializer();

        // Case 1: Global is a struct with function pointer fields
        if (auto *CS = dyn_cast<ConstantStruct>(Init)) {
            const StructType *ST = dyn_cast<StructType>(GV.getValueType());
            if (!ST) continue;

            std::string StructTypeName;
            if (ST->hasName())
                StructTypeName = ST->getName().str();

            for (unsigned i = 0; i < CS->getNumOperands(); ++i) {
                Value *op = CS->getOperand(i);

                if (Function *F = dyn_cast<Function>(op)) {
                    // Record function pointer assignment from struct initializer
                    FunctionPointerSettingInfo settingInfo;
                    settingInfo.ModName = ModName;
                    settingInfo.VarName = GV.getName().str();  // Variable name from global
                    settingInfo.SetterName = "global";
                    settingInfo.StructTypeName = StructTypeName;
                    settingInfo.FuncName = F->getName().str();
                    settingInfo.Line = 0;  // No line info for static globals
                    settingInfo.Offset = i;

                    std::string key = ModName + ":0";  // Use line 0 as placeholder
                    FunctionPointerSettings[key].push_back(settingInfo);
                }
            }
        }

        // Case 2: Global variable directly initialized with a function
        if (Function *F = dyn_cast<Function>(Init)) {
            FunctionPointerSettingInfo settingInfo;
            settingInfo.ModName = ModName;
            settingInfo.VarName = GV.getName().str();  // Variable name
            settingInfo.SetterName = "global";
            settingInfo.StructTypeName = "";  // Not part of a struct
            settingInfo.FuncName = F->getName().str();
            settingInfo.Line = 0;
            settingInfo.Offset = 0;

            std::string key = ModName + ":0";
            FunctionPointerSettings[key].push_back(settingInfo);
        }
    }
}


void CallGraphPass::RecordFunctionPointerSetting(
    const std::string &ModName,
    const std::string &SetterName,
    const std::string &StructTypeName,
    const std::string &FuncName,
    unsigned Line,
    unsigned Offset) {

    // Check if the function pointer has already been recorded for this module and line
    if (ProcessedSettings.find({ModName, FuncName, Line, Offset}) != ProcessedSettings.end()) {
        return;  // Skip if already processed
    }

    // Record the setting
    FunctionPointerSettingInfo settingInfo;
    settingInfo.ModName = ModName;
    settingInfo.SetterName = SetterName;
    settingInfo.StructTypeName = StructTypeName;
    settingInfo.FuncName = FuncName;
    settingInfo.Line = Line;
    settingInfo.Offset = Offset;

    // Insert the setting info into the appropriate map, grouped by module name and line
    FunctionPointerSettings[ModName + ":" + std::to_string(Line)].push_back(settingInfo);

    // Add the setting to the processed set to avoid future duplication
    ProcessedSettings.insert({ModName, FuncName, Line, Offset});

    // Log the addition of the function pointer setting
    errs() << "[debug] Found function pointer setting: " << SetterName
           << " in module " << ModName << " at line " << Line
           << " for function " << FuncName << " with offset " << Offset << "\n";
}

void CallGraphPass::CollectCallingAddressTakenFunction(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *call = dyn_cast<CallInst>(&I)) {
                    Value *calledValue = call->getCalledOperand()->stripPointerCasts();

                    // Skip direct function calls
                    if (isa<Function>(calledValue)) continue;

                    std::string varName = "";
                    unsigned offset = 0;

                    // Try to extract variable name and offset used in the indirect call
                    if (auto *load = dyn_cast<LoadInst>(calledValue)) {
                        Value *ptr = load->getPointerOperand();

                        // Local variable case (alloca)
                        if (auto *alloca = dyn_cast<AllocaInst>(ptr)) {
                            varName = alloca->getName().str();
                        }
                        // Global variable case
                        else if (auto *global = dyn_cast<GlobalVariable>(ptr)) {
                            varName = global->getName().str();
                        }
                    }

                    // For direct global loads like @sfp
                    if (auto *global = dyn_cast<GlobalVariable>(calledValue)) {
                        varName = global->getName().str();
                    }

                    // Set offset if dealing with struct (e.g., .foo, .bar)
                    if (auto *gep = dyn_cast<GetElementPtrInst>(calledValue)) {
                        offset = 0; // You can refine the logic to get the specific offset for each field
                    }

                    unsigned line = getLineNumber(call);
                    RecordCallGraphEdge(ModName, F.getName().str(), "indirect", line, true, varName, offset);

                    errs() << "[debug] Recorded indirect call: " << F.getName()
                           << " -> indirect (line: " << line << ")"
                           << " via variable: " << varName << " with offset: " << offset << " in module: " << ModName << "\n";
                }
            }
        }
    }
}

void CallGraphPass::CollectDynamicFunctionPointerAssignments(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        if (F.isDeclaration()) continue;

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *store = dyn_cast<StoreInst>(&I)) {
                    Value *val = store->getValueOperand()->stripPointerCasts();
                    if (Function *Fptr = dyn_cast<Function>(val)) {
                        unsigned Line = 0;
                        if (DILocation *Loc = I.getDebugLoc())
                            Line = Loc->getLine();

                        std::string Setter = F.getName().str();
                        RecordFunctionPointerSetting(ModName, Setter, "", Fptr->getName().str(), Line, 0);
                    }
                }
            }
        }
    }
}

void CallGraphPass::CollectFunctionPointerArgumentPassing(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        if (F.isDeclaration()) continue;

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *call = dyn_cast<CallBase>(&I)) {
                    Function *calledFunc = dyn_cast<Function>(call->getCalledOperand()->stripPointerCasts());
                    if (!calledFunc) continue;

                    for (unsigned i = 0; i < call->arg_size(); ++i) {
                        Value *arg = call->getArgOperand(i)->stripPointerCasts();
                        if (Function *passedFunc = dyn_cast<Function>(arg)) {
                            unsigned Line = 0;
                            if (DILocation *Loc = I.getDebugLoc())
                                Line = Loc->getLine();

                            std::string Caller = F.getName().str();
                            std::string Callee = passedFunc->getName().str();

                            // Record the function pointer call via argument
                            RecordFunctionPointerCall(ModName, Caller, Callee, Line, i);
                        }
                    }
                }
            }
        }
    }
}

void CallGraphPass::CollectDirectCalls(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        if (F.isDeclaration()) continue;

        std::string CallerFunc = F.getName().str();

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                // Look for direct call instructions
                if (auto *call = dyn_cast<CallBase>(&I)) {
                    // Resolve the callee function
                    Function *calleeFunc = dyn_cast<Function>(call->getCalledOperand()->stripPointerCasts());

                    // Skip if we can't resolve the callee at all (e.g., function pointer)
                    if (!calleeFunc) continue;

                    std::string CalleeFunc = calleeFunc->getName().str();
                    unsigned Line = 0;
                    if (DILocation *Loc = I.getDebugLoc())
                        Line = Loc->getLine();

                    // Record all resolved direct calls, including external functions like printf
                    RecordCallGraphEdge(ModName, CallerFunc, CalleeFunc, Line, false);
                }
            }
        }
    }
}


void CallGraphPass::AnalyzeIndirectCalls() {
    for (const auto &entry : FunctionPointerUses) {
        const std::string &key = entry.first;
        const std::vector<FunctionPointerUseInfo> &uses = entry.second;

        for (const auto &use : uses) {
            // Create a call edge with "indirect" as the callee function
            RecordCallGraphEdge(
                use.ModName,
                use.CallerFuncName,
                "indirect",  // Placeholder
                use.Line,
                true         // IsIndirect
            );
        }
    }
}

void CallGraphPass::ResolveIndirectCalls() {
    for (auto &modEntry : CallGraph) {
        std::string ModName = modEntry.first;
        std::vector<CallEdgeInfo> &edges = modEntry.second;

        for (auto &edge : edges) {
            // Only process unresolved indirect calls
            if (!edge.IsIndirect || edge.CalleeFunction != "indirect")
                continue;

            std::string bestMatch = "";

            // Find corresponding FunctionPointerUseInfo
            for (const auto &useEntry : FunctionPointerUses) {
                const std::vector<FunctionPointerUseInfo> &uses = useEntry.second;
                for (const auto &use : uses) {
                    if (use.ModName != edge.CallerModule ||
                        use.CallerFuncName != edge.CallerFunction ||
                        use.Line != edge.Line)
                        continue;

                    // Match this use against FunctionPointerCallMap
                    for (const auto &callEntry : FunctionPointerCalls) {
                        const std::vector<FunctionPointerCallInfo> &calls = callEntry.second;
                        for (const auto &call : calls) {
                            // Match on module and argument index
                            if (call.ModName == use.ModName &&
                                call.ArgIndex == use.ArgIndex) {
                                bestMatch = call.CalleeFuncName;
                                break;
                            }
                        }
                        if (!bestMatch.empty()) break;
                    }
                }
                if (!bestMatch.empty()) break;
            }

            // If match found, update the CalleeFunction
            if (!bestMatch.empty()) {
                errs() << "[debug] Resolved indirect call at "
                       << edge.CallerFunction << ":" << edge.Line
                       << " to " << bestMatch << "\n";
                edge.CalleeFunction = bestMatch;
            }
        }
    }
}

void CallGraphPass::AnalyzeStaticFPCallSites() {
    for (auto &modEntry : CallGraph) {
        std::string ModName = modEntry.first;
        std::vector<CallEdgeInfo> &edges = modEntry.second;

        for (auto &edge : edges) {
            if (!edge.IsIndirect || edge.CalleeFunction != "indirect")
                continue;

            const std::string &varName = edge.VarName;
            unsigned offset = edge.Offset;

            if (varName.empty())
                continue;

            // Search all entries in FunctionPointerSettings
            for (const auto &entry : FunctionPointerSettings) {
                for (const auto &info : entry.second) {
                    // Ensure we are in the same module and match by VarName and Offset
                    if (info.ModName != ModName) continue;
                    if (info.VarName != varName) continue;
                    if (info.Offset != offset) continue;  // Match by Offset

                    // Match found, resolve the function
                    edge.CalleeFunction = info.FuncName;

                    errs() << "[debug] Resolved indirect call at "
                           << edge.CallerFunction << ":" << edge.Line
                           << " to " << info.FuncName
                           << " via variable: " << varName
                           << " with offset: " << offset << "\n";
                    break;
                }
            }
        }
    }
}

void CallGraphPass::AnalyzeStaticGlobalFPCalls() {
    for (auto &modEntry : CallGraph) {
        std::string ModName = modEntry.first;
        std::vector<CallEdgeInfo> &edges = modEntry.second;

        for (auto &edge : edges) {
            if (!edge.IsIndirect || edge.CalleeFunction != "indirect")
                continue;

            const std::string &varName = edge.VarName;

            if (varName.empty())
                continue;

            // Search all entries in FunctionPointerSettings
            for (const auto &entry : FunctionPointerSettings) {
                for (const auto &info : entry.second) {
                    // Must be from same module, not a struct assignment, and matching variable name
                    if (info.ModName != ModName)
                        continue;
                    if (!info.StructTypeName.empty())
                        continue;
                    if (info.VarName != varName)
                        continue;

                    // Match found — update callee function name
                    edge.CalleeFunction = info.FuncName;

                    errs() << "[debug] Resolved indirect call at "
                           << edge.CallerFunction << ":" << edge.Line
                           << " to " << info.FuncName
                           << " via global variable: " << info.VarName << "\n";
                    break;
                }
            }
        }
    }
}

void CallGraphPass::RecordFunctionPointerCall(
    const std::string &ModName, 
    const std::string &CallerFuncName, 
    const std::string &CalleeFuncName, 
    unsigned Line,
    unsigned ArgIndex) {

    std::string key = ModName + ":" + std::to_string(Line) + ":" + std::to_string(ArgIndex);

    // Create a FunctionPointerCallInfo object
    FunctionPointerCallInfo callInfo{ModName, CallerFuncName, CalleeFuncName, Line, ArgIndex};

    // Insert the call information into the map with the updated key
    FunctionPointerCalls[key].push_back(callInfo);

    // Optionally log the function pointer call information
    errs() << "[debug] Recorded function pointer call: " 
           << "Module: " << ModName 
           << ", Caller: " << CallerFuncName 
           << ", Callee: " << CalleeFuncName 
           << " at line: " << Line
           << " with argument index: " << ArgIndex << "\n";
}

void CallGraphPass::RecordFunctionPointerUse(
    const std::string &ModName,
    const std::string &CallerFuncName,
    const std::string &CalleeFuncName,
    unsigned Line,
    unsigned ArgIndex) {

    std::string key = ModName + ":" + std::to_string(Line) + ":" + std::to_string(ArgIndex);
    FunctionPointerUseInfo info{ModName, CallerFuncName, CalleeFuncName, Line, ArgIndex};
    FunctionPointerUses[key].push_back(info);

    errs() << "[debug] Recorded function pointer use: Module: " << ModName
           << ", Caller: " << CallerFuncName << ", Callee: " << CalleeFuncName
           << " at line: " << Line << " with argument index: " << ArgIndex << "\n";
}

void CallGraphPass::RecordCallGraphEdge(
    const std::string &ModName,
    const std::string &CallerFunc,
    const std::string &CalleeFunc,
    unsigned Line,
    bool IsIndirect,
    const std::string &VarName,
    unsigned Offset) {

    CallEdgeInfo edge;
    edge.CallerModule = ModName;
    edge.CallerFunction = CallerFunc;
    edge.CalleeFunction = CalleeFunc;
    edge.Line = Line;
    edge.IsIndirect = IsIndirect;
    edge.VarName = VarName;
    edge.Offset = Offset;

    CallGraph[ModName].push_back(edge);

    // Debug print
    errs() << "[debug] Recorded " << (IsIndirect ? "indirect" : "direct") << " call: "
           << CallerFunc << " -> " << CalleeFunc
           << " (line: " << Line << ") in module: " << ModName << "\n";
}
