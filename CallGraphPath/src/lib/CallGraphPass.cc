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

void IterativeModulePass::run(ModuleList &modules) {
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
    // Get the module name (e.g., file path of the .bc file)
    std::string ModName = M->getName().str();

	errs() << "Collecting information from module: " << ModName << "\n";

	CollectFunctionProtoTypes(M);
	CollectStaticFunctionPointerInit(M);
	CollectDynamicFunctionPointerInit(M);
    CollectIndirectCallCandidates(M);
    CollectGlobalFunctionPointerInit(M);
    CollectFunctionPointerArguments(M);
    CollectCallInstructions(M);

    return true;
}


bool CallGraphPass::IdentifyTargets(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        if (F.isDeclaration()) continue;

        std::string Caller = F.getName().str();

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *call = dyn_cast<CallBase>(&I)) {
                    Value *calledVal = call->getCalledOperand()->stripPointerCasts();

                    // Skip if it's a direct function call
                    if (isa<Function>(calledVal)) {
                        continue;
                    }

                    unsigned Line = 0;
                    if (DILocation *Loc = I.getDebugLoc())
                        Line = Loc->getLine();

                    if (Function *target = resolveCalledFunction(calledVal)) {
                        IndirectCallCandidates[ModName][Caller][Line].insert(target->getName().str());
                    }
                }
            }
        }
    }

    PrintResolvedIndirectCalls(IndirectCallCandidates);
    return true;
}

void CallGraphPass::CollectFunctionProtoTypes(Module *M) {
	std::string ModName = M->getName().str();
	// Vector to store function prototype strings
	std::vector<std::string> FuncPrototypes;

	// Iterate over all functions in the module
	for (Function &F : M->functions()) {
		// Skip function declarations (no body)
		if (F.isDeclaration()) continue;

		std::string Proto;
		raw_string_ostream RSO(Proto);

		// Function name
		RSO << F.getName() << ":";

		// Return type
		F.getReturnType()->print(RSO);
		RSO << ":";

		// Argument types
		for (unsigned i = 0; i < F.arg_size(); ++i) {
			if (i > 0) RSO << ",";
			F.getFunctionType()->getParamType(i)->print(RSO);
		}

        unsigned Line = 0;
        if (DISubprogram *SP = F.getSubprogram()) {
            Line = SP->getLine();
        }

        RSO << ":" << Line;

		// Add to prototype list
		if (!RSO.str().empty()) {
			FuncPrototypes.push_back(RSO.str());
		}
	}

	// Save function prototypes under module name
	ModuleFuncMap[ModName] = std::move(FuncPrototypes);
}

void CallGraphPass::CollectStaticFunctionPointerInit(Module *M) {
    for (GlobalVariable &GV : M->globals()) {
        // errs() << "Processing global variable: " << GV.getName() << "\n";

        if (!GV.hasInitializer()) {
            // errs() << "  (no initializer)\n";
            continue;
        }

        Constant *Init = GV.getInitializer();

        // Check if the initializer is a ConstantStruct
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

                // Case 1: direct function pointer
                if (Function *F = dyn_cast<Function>(op)) {
                    // Extract struct type name from GlobalVariable type
                    Type *ElemType = GV.getValueType(); // should be StructType*
                    std::string StructTypeName = "unknown_struct_type";
                    if (StructType *ST = dyn_cast<StructType>(ElemType)) {
                        if (ST->hasName())
                            StructTypeName = ST->getName().str();
                    }

                    // Register to StaticFPMap
                    RecordStaticFuncPtrInit(StructTypeName, GV.getName(), i, F->getName(), Line);
                }

                // Case 2: bitcasted function pointer
                else if (auto *CE = dyn_cast<ConstantExpr>(op)) {
                    if (CE->isCast()) {
                        if (Function *F = dyn_cast<Function>(CE->getOperand(0))) {
                            std::string StructTypeName = "unknown_struct_type";
                            Type *ElemType = GV.getValueType();
                            if (StructType *ST = dyn_cast<StructType>(ElemType)) {
                                if (ST->hasName())
                                    StructTypeName = ST->getName().str();
                            }

                            RecordStaticFuncPtrInit(StructTypeName, GV.getName(), i, F->getName(), Line);
                        }
                    }
                }
            }
        } else {
            // errs() << "  (Unhandled initializer type: " << Init->getValueID() << ")\n";
        }
    }
}

void CallGraphPass::CollectDynamicFunctionPointerInit(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        if (F.isDeclaration()) continue;

        std::string FuncName = F.getName().str();

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *store = dyn_cast<StoreInst>(&I)) {
                    Value *val = store->getValueOperand();
                    Value *ptr = store->getPointerOperand();

                    Function *TargetFunc = nullptr;

                    // Case 1: direct function pointer
                    if ((TargetFunc = dyn_cast<Function>(val))) {
                        // nothing to do
                    }
                    // Case 2: bitcasted function pointer
                    else if (auto *CE = dyn_cast<ConstantExpr>(val)) {
                        if (CE->isCast()) {
                            if (Function *F = dyn_cast<Function>(CE->getOperand(0))) {
                                TargetFunc = F;
                            }
                        }
                    }

                    if (TargetFunc) {
                        std::string PtrStr;
                        raw_string_ostream RSO(PtrStr);
                        ptr->print(RSO);

                        // Attempt to get debug line number if available
                        unsigned Line = 0;
                        if (DILocation *Loc = I.getDebugLoc()) {
                            Line = Loc->getLine();
                        }

                        RecordDynamicFuncPtrAssignment(
                            ModName, FuncName, TargetFunc->getName(), RSO.str(), Line);
                    }
                }
            }
        }
    }
}

void CallGraphPass::CollectGlobalFunctionPointerInit(Module *M) {
    std::string ModName = M->getName().str();

    for (GlobalVariable &GV : M->globals()) {
        if (!GV.hasInitializer()) continue;

        Constant *Init = GV.getInitializer();
        Function *TargetFunc = nullptr;

        // Case 1: direct function pointer
        if ((TargetFunc = dyn_cast<Function>(Init))) {
            // OK
        }

        // Case 2: bitcasted function
        else if (auto *CE = dyn_cast<ConstantExpr>(Init)) {
            if (CE->isCast()) {
                if (Function *F = dyn_cast<Function>(CE->getOperand(0))) {
                    TargetFunc = F;
                }
            }
        }

        if (TargetFunc) {
            // LHS is global variable name (GV.getName())
            RecordDynamicFuncPtrAssignment(
                ModName, "__global__", TargetFunc->getName(), GV.getName(), 0);
        }
    }
}

void CallGraphPass::CollectFunctionPointerArguments(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        if (F.isDeclaration()) continue;

        std::string CallerName = F.getName().str();

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *call = dyn_cast<CallBase>(&I)) {
                    Function *Callee = dyn_cast<Function>(call->getCalledOperand()->stripPointerCasts());
                    if (!Callee) continue;

                    for (unsigned i = 0; i < call->arg_size(); ++i) {
                        Value *Arg = call->getArgOperand(i);
                        Function *PassedFunc = nullptr;

                        if ((PassedFunc = dyn_cast<Function>(Arg))) {
                            // ok
                        } else if (auto *CE = dyn_cast<ConstantExpr>(Arg)) {
                            if (CE->isCast()) {
                                PassedFunc = dyn_cast<Function>(CE->getOperand(0));
                            }
                        }

                        if (PassedFunc) {
                            unsigned Line = 0;
                            if (DILocation *Loc = I.getDebugLoc()) {
                                Line = Loc->getLine();
                            }

                            RecordFuncPtrArgument(
                                ModName,
                                CallerName,
                                PassedFunc->getName(),
                                Callee->getName(),
                                i,
                                Line
                            );
                        }
                    }
                }
            }
        }
    }
}

void CallGraphPass::CollectIndirectCallCandidates(Module *M) {
    std::string ModName = M->getName().str();

    for (Function &F : *M) {
        if (F.isDeclaration()) continue;

        std::string FuncName = F.getName().str();

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *call = dyn_cast<CallBase>(&I)) {
                    Value *called = call->getCalledOperand();

                    // Skip direct calls
                    if (isa<Function>(called)) continue;

                    std::string AssignedFromStr;

                    // Try to trace back if this is a value loaded from a pointer
                    if (Instruction *calledInst = dyn_cast<Instruction>(called)) {
                        if (auto *load = dyn_cast<LoadInst>(calledInst)) {
                            Value *loadedFrom = load->getPointerOperand();
                            raw_string_ostream RSO(AssignedFromStr);
                            loadedFrom->print(RSO);
                            RSO.flush();
                        } else {
                            // Not a load, just print operand
                            raw_string_ostream RSO(AssignedFromStr);
                            called->print(RSO);
                            RSO.flush();
                        }
                    } else {
                        // Not an instruction, just print operand
                        raw_string_ostream RSO(AssignedFromStr);
                        called->print(RSO);
                        RSO.flush();
                    }

                    // Get debug line number if available
                    unsigned Line = 0;
                    if (DILocation *Loc = I.getDebugLoc()) {
                        Line = Loc->getLine();
                    }

                    std::set<std::string> CandidateFuncs;

                    // Search DynamicFPMap for matching pointer assignments
                    auto modIt = DynamicFPMap.find(ModName);
                    if (modIt != DynamicFPMap.end()) {
                        auto funcIt = modIt->second.find(FuncName);
                        if (funcIt != modIt->second.end()) {
                            for (const std::string &entry : funcIt->second) {
                                // entry format: "foo:%ptr:line"
                                size_t colon1 = entry.find(':');
                                size_t colon2 = entry.find(':', colon1 + 1);
                                if (colon1 == std::string::npos || colon2 == std::string::npos)
                                    continue;

                                std::string targetFunc = entry.substr(0, colon1);
                                std::string assignedTo = entry.substr(colon1 + 1, colon2 - colon1 - 1);

                                if (assignedTo == AssignedFromStr) {
                                    CandidateFuncs.insert(targetFunc);
                                }
                            }
                        }
                    }

                    // Store or print result
                    // errs() << ModName << ": " << FuncName << " [line " << Line << "] -> ";
                    // if (CandidateFuncs.empty()) {
                    //     errs() << "(unknown)\n";
                    // } else {
                    //     errs() << "[";
                    //     bool first = true;
                    //     for (const auto &name : CandidateFuncs) {
                    //         if (!first) errs() << ", ";
                    //         errs() << name;
                    //         first = false;
                    //     }
                    //     errs() << "]\n";
                    // }

                    IndirectCallCandidates[ModName][FuncName][Line] = CandidateFuncs;
                }
            }
        }
    }
}

void CallGraphPass::RecordStaticFuncPtrInit(StringRef StructTypeName, StringRef VarName,
	unsigned Index, StringRef FuncName, unsigned Line) {
	std::string Entry = std::to_string(Index) + ":" + FuncName.str() + ":" + std::to_string(Line);
	StaticFPMap[StructTypeName.str()][VarName.str()].push_back(Entry);
}

void CallGraphPass::RecordDynamicFuncPtrAssignment(
    StringRef ModuleName,
    StringRef InFunction,
    StringRef TargetFunc,
    StringRef AssignedTo,
    unsigned LineNumber)
{
    std::string Entry = TargetFunc.str() + ":" + AssignedTo.str() + ":" + std::to_string(LineNumber);
    DynamicFPMap[ModuleName.str()][InFunction.str()].insert(Entry);
}

void CallGraphPass::RecordFuncPtrArgument(
    StringRef ModuleName,
    StringRef CallerFunc,
    StringRef PassedFunc,
    StringRef CalleeFunc,
    unsigned ArgIndex,
    unsigned LineNumber)
{
    std::string Entry = PassedFunc.str() + ":arg" + std::to_string(ArgIndex) +
                        ":" + CalleeFunc.str() + ":" + std::to_string(LineNumber);
    FuncPtrArgMap[ModuleName.str()][CallerFunc.str()].push_back(Entry);
}

void CallGraphPass::CollectCallInstructions(Module *M) {
    std::string ModName = M->getName().str();

    // Iterate through all functions in the module
    for (Function &F : *M) {
        if (F.isDeclaration()) continue; // Skip function declarations

        std::string CallerName = F.getName().str();

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                // Only process call and invoke instructions
                if (auto *call = dyn_cast<CallBase>(&I)) {
                    // Try to get line number from debug info
                    unsigned Line = 0;
                    if (DILocation *Loc = I.getDebugLoc()) {
                        Line = Loc->getLine();
                    }

                    // Get the called value (may be direct or indirect)
                    Value *calledVal = call->getCalledOperand()->stripPointerCasts();

                    if (Function *CalleeFunc = dyn_cast<Function>(calledVal)) {
                        // Direct call detected
                        RecordDirectCall(ModName, CallerName, CalleeFunc->getName(), Line);
                    } else {
                        // Indirect call (e.g. through function pointer)
                        std::string CalledExpr;
                        raw_string_ostream RSO(CalledExpr);
                        calledVal->print(RSO);
                        RecordIndirectCall(ModName, CallerName, RSO.str(), Line);
                    }
                }
            }
        }
    }
}

void CallGraphPass::RecordDirectCall(StringRef ModuleName,
    StringRef CallerFunc,
    StringRef CalleeFunc,
    unsigned Line) {
    // Create a string like "callee_name:line_number"
    std::string Entry = CalleeFunc.str() + ":" + std::to_string(Line);

    // Store into the DirectCallMap
    DirectCallMap[ModuleName.str()][CallerFunc.str()].push_back(Entry);
}

void CallGraphPass::RecordIndirectCall(StringRef ModuleName,
    StringRef CallerFunc,
    StringRef CalledValueStr,
    unsigned Line) {
    // Create a string like "called_expression:line_number"
    std::string Entry = CalledValueStr.str() + ":" + std::to_string(Line);

    // Store into the IndirectCallMap
    IndirectCallMap[ModuleName.str()][CallerFunc.str()].push_back(Entry);
}

Function* CallGraphPass::resolveCalledFunction(Value *V) {
    // Case 1: already a direct function pointer
    if (Function *F = dyn_cast<Function>(V)) {
        return F;
    }

    // Case 2: load instruction (e.g., %1 = load ptr, ptr %fp)
    if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
        Value *ptr = LI->getPointerOperand();

        // Search for store instruction to that pointer
        for (User *U : ptr->users()) {
            if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
                Value *storedVal = SI->getValueOperand();

                if (Function *F = dyn_cast<Function>(storedVal)) {
                    return F;
                }

                // If bitcasted
                if (auto *CE = dyn_cast<ConstantExpr>(storedVal)) {
                    if (CE->isCast()) {
                        if (Function *F = dyn_cast<Function>(CE->getOperand(0))) {
                            return F;
                        }
                    }
                }
            }
        }
    }

    return nullptr; // Not resolved in this simple version
}
