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
    
    PrintModuleFunctionMap(ModuleFunctionMap, M->getName().str());
    PrintFunctionPointerSettings(FunctionPointerSettings);
    PrintFunctionPointerCallMap(FunctionPointerCallMap);
    return true;
}

bool CallGraphPass::IdentifyTargets(Module *M) {
    std::string ModName = M->getName().str();
    errs() << "Identifying targets in module: " << ModName << "\n";

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
    for (GlobalVariable &GV : M->globals()) {
        if (!GV.hasInitializer()) continue;

        Constant *Init = GV.getInitializer();
        
        // Case 1: Direct function pointer assignments (e.g., fp = foo;)
        if (auto *funcPtr = dyn_cast<Function>(Init)) {
            unsigned Line = 0;
            if (GV.hasMetadata()) {
                if (auto *dbg = GV.getMetadata("dbg")) {
                    if (auto *DGV = dyn_cast<DIGlobalVariableExpression>(dbg)) {
                        Line = DGV->getVariable()->getLine();
                    }
                }
            }

            // Register the function pointer assignment for direct function pointers
            RecordFunctionPointerSetting(M->getName().str(), GV.getName().str(), "", funcPtr->getName().str(), Line, 0);
        }

        // Case 2: Handle function pointer assignments inside structures (e.g., struct with function pointers)
        if (auto *CS = dyn_cast<ConstantStruct>(Init)) {
            for (unsigned i = 0; i < CS->getNumOperands(); ++i) {
                Value *op = CS->getOperand(i);
                unsigned Line = 0;
                if (GV.hasMetadata()) {
                    if (auto *dbg = GV.getMetadata("dbg")) {
                        if (auto *DGV = dyn_cast<DIGlobalVariableExpression>(dbg)) {
                            Line = DGV->getVariable()->getLine();
                        }
                    }
                }

                // Case 1: Direct function pointer inside struct
                if (Function *F = dyn_cast<Function>(op)) {
                    // Extract struct type name and offset from GlobalVariable type
                    Type *ElemType = GV.getValueType(); // Should be StructType*
                    unsigned Offset = 0; // Calculate the offset
                    std::string StructTypeName = "unknown"; // Default if StructTypeName is not found
                    
                    if (StructType *ST = dyn_cast<StructType>(ElemType)) {
                        if (ST->hasName()) {
                            StructTypeName = ST->getName().str();  // Get the name of the struct
                        }
                        Offset = i * 8;  // Assuming 8-byte function pointer size
                    }

                    // Register to StaticFPMap with offset
                    RecordFunctionPointerSetting(M->getName().str(), GV.getName().str(), StructTypeName, F->getName().str(), Line, Offset);
                }

                // Case 2: Function pointer assignment via bitcast (e.g., bitcast function pointer)
                else if (auto *CE = dyn_cast<ConstantExpr>(op)) {
                    if (CE->isCast()) {
                        if (Function *F = dyn_cast<Function>(CE->getOperand(0))) {
                            std::string StructTypeName = "unknown";
                            Type *ElemType = GV.getValueType();
                            if (StructType *ST = dyn_cast<StructType>(ElemType)) {
                                if (ST->hasName())
                                    StructTypeName = ST->getName().str();
                            }

                            // Register to StaticFPMap with offset
                            RecordFunctionPointerSetting(M->getName().str(), GV.getName().str(), StructTypeName, F->getName().str(), Line, i * 8);
                        }
                    }
                }
            }
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
        if (F.isDeclaration()) continue;

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *call = dyn_cast<CallBase>(&I)) {
                    Value *calledValue = call->getCalledOperand()->stripPointerCasts();

                    // Skip direct calls
                    if (isa<Function>(calledValue) || isa<ConstantExpr>(calledValue)) {
                        continue;
                    }

                    // Get line number
                    unsigned Line = 0;
                    if (DILocation *Loc = I.getDebugLoc()) {
                        Line = Loc->getLine();
                    }

                    std::string CallerFuncName = F.getName().str();
                    std::string CalleeFuncName = "indirect";

                    // Case 1: Called value is directly an argument
                    for (unsigned i = 0; i < F.arg_size(); ++i) {
                        Argument &arg = *(F.arg_begin() + i);
                        if (&arg == calledValue) {
                            // Directly using argument as function pointer
                            RecordFunctionPointerCall(ModName, CallerFuncName, CalleeFuncName, Line, i);
                            goto done; // skip to next instruction
                        }
                    }

                    // Case 2: Called value is loaded from memory (possibly from a function pointer argument)
                    if (auto *loadInst = dyn_cast<LoadInst>(calledValue)) {
                        Value *loadedFrom = loadInst->getPointerOperand()->stripPointerCasts();

                        for (User *U : loadedFrom->users()) {
                            if (auto *storeInst = dyn_cast<StoreInst>(U)) {
                                Value *storedVal = storeInst->getValueOperand()->stripPointerCasts();

                                for (unsigned i = 0; i < F.arg_size(); ++i) {
                                    Argument &arg = *(F.arg_begin() + i);
                                    if (&arg == storedVal) {
                                        // Indirect call via function pointer argument stored in local variable
                                        RecordFunctionPointerCall(ModName, CallerFuncName, CalleeFuncName, Line, i);
                                        goto done;
                                    }
                                }
                            }
                        }
                    }

                    // Optional: fallback for unknown indirect call
                    RecordFunctionPointerCall(ModName, CallerFuncName, CalleeFuncName, Line, 0);

                done:
                    continue;
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
    FunctionPointerCallMap[key].push_back(callInfo);

    // Optionally log the function pointer call information
    errs() << "[debug] Recorded function pointer call: " 
           << "Module: " << ModName 
           << ", Caller: " << CallerFuncName 
           << ", Callee: " << CalleeFuncName 
           << " at line: " << Line
           << " with argument index: " << ArgIndex << "\n";
}
