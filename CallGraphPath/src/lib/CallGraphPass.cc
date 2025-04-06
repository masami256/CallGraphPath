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
    CollectGlobalFunctionPointerInit(M);
    CollectFunctionPointerArguments(M);

    return true;
}


bool CallGraphPass::IdentifyTargets(Module *M) {
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

void CallGraphPass::RecordStaticFuncPtrInit(StringRef StructTypeName, StringRef VarName,
	unsigned Index, StringRef FuncName, unsigned Line) {
	std::string Entry = std::to_string(Index) + ":" + FuncName.str() + ":" + std::to_string(Line);
	StaticFPMap[StructTypeName.str()][VarName.str()].push_back(Entry);
}

void CallGraphPass::RecordDynamicFuncPtrAssignment(StringRef ModuleName, StringRef InFunction,
	StringRef TargetFunc, StringRef AssignedTo, unsigned LineNumber) {
    std::string Entry = TargetFunc.str() + ":" + AssignedTo.str() + ":" + std::to_string(LineNumber);
    DynamicFPMap[ModuleName.str()][InFunction.str()].push_back(Entry);
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
