#include "CallGraphPass.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/User.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <list>

using namespace llvm;

ModuleFunctionMap ModuleFuncMap;
StaticFunctionPointerMap StaticFPMap;

static void PrintModuleFunctionMap(ModuleFunctionMap &ModuleFunctionMap) {
    std::cout << "=== ModuleFunctionMap Debug Dump ===" << std::endl;

    // Iterate over each module
    for (const auto &entry : ModuleFunctionMap) {
        const std::string &moduleName = entry.first;
        const std::vector<std::string> &functions = entry.second;

        std::cout << "Module: " << moduleName << std::endl;

        // Iterate over function prototype strings
        for (const std::string &proto : functions) {
            std::cout << "  [" << proto << "]" << std::endl;
        }

        std::cout << std::endl;
    }

    std::cout << "=== End of Dump ===" << std::endl;
}

void PrintStaticFunctionPointerMap() {
    std::cout << "==== Static Function Pointer Map ====" << std::endl;

    // Iterate over struct types
    for (const auto &typeEntry : StaticFPMap) {
        const std::string &structType = typeEntry.first;
        const auto &instanceMap = typeEntry.second;

        std::cout << "Struct Type: " << structType << std::endl;

        // Iterate over each global variable (instance of the struct)
        for (const auto &varEntry : instanceMap) {
            const std::string &varName = varEntry.first;
            const std::vector<std::string> &entries = varEntry.second;

            std::cout << "  Variable: " << varName << std::endl;

            // Show index:function mappings
            for (const std::string &entry : entries) {
                std::cout << "    " << entry << std::endl;
            }
        }

        std::cout << std::endl;
    }

    std::cout << "==== End of Map ====" << std::endl;
}

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

	PrintStaticFunctionPointerMap();
    std::cout << "Pass completed: " << ID << std::endl;
}

bool CallGraphPass::CollectInformation(Module *M) {
    // Get the module name (e.g., file path of the .bc file)
    std::string ModName = M->getName().str();

	errs() << "Collecting information from module: " << ModName << "\n";

	CollectFunctionProtoTypes(M);
	CollectStaticFunctionPointerInit(M);
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
        errs() << "Processing global variable: " << GV.getName() << "\n";

        if (!GV.hasInitializer()) {
            // errs() << "  (no initializer)\n";
            continue;
        }

        Constant *Init = GV.getInitializer();

        // Check if the initializer is a ConstantStruct
        if (auto *CS = dyn_cast<ConstantStruct>(Init)) {
            for (unsigned i = 0; i < CS->getNumOperands(); ++i) {
                Value *op = CS->getOperand(i);

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
                    RecordStaticFuncPtrInit(StructTypeName, GV.getName(), i, F->getName());
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

                            RecordStaticFuncPtrInit(StructTypeName, GV.getName(), i, F->getName());
                        }
                    }
                }
            }
        } else {
            // errs() << "  (Unhandled initializer type: " << Init->getValueID() << ")\n";
        }
    }
}


void CallGraphPass::RecordStaticFuncPtrInit(StringRef StructTypeName, StringRef VarName,
	unsigned Index, StringRef FuncName) {
	std::string Entry = std::to_string(Index) + ":" + FuncName.str();
	StaticFPMap[StructTypeName.str()][VarName.str()].push_back(Entry);
}
