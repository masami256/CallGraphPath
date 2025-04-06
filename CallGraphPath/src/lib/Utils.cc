#include "Utils.h"
#include <iostream>

void PrintFuncPtrArgumentMap(FunctionPtrArgMap &FuncPtrArgMap) {
    std::cout << "==== Function Pointer Argument Map ====" << std::endl;

    for (const auto &modEntry : FuncPtrArgMap) {
        const std::string &modName = modEntry.first;
        const auto &funcMap = modEntry.second;

        std::cout << "Module: " << modName << std::endl;

        for (const auto &funcEntry : funcMap) {
            const std::string &caller = funcEntry.first;
            const std::vector<std::string> &entries = funcEntry.second;

            std::cout << "  Caller Function: " << caller << std::endl;
            for (const std::string &entry : entries) {
                std::cout << "    " << entry << std::endl;
            }
        }
    }

    std::cout << "==== End of Map ====" << std::endl;
}

void PrintDynamicFunctionPointerMap(DynamicFunctionPointerMap &DynamicFPMap) {
    std::cout << "==== Dynamic Function Pointer Map ====" << std::endl;

    for (const auto &modEntry : DynamicFPMap) {
        const std::string &modName = modEntry.first;
        const auto &funcMap = modEntry.second;

        std::cout << "Module: " << modName << std::endl;

        for (const auto &funcEntry : funcMap) {
            const std::string &funcName = funcEntry.first;
            const std::vector<std::string> &entries = funcEntry.second;

            std::cout << "  Function: " << funcName << std::endl;

            for (const std::string &entry : entries) {
                std::cout << "    " << entry << std::endl;
            }
        }

        std::cout << std::endl;
    }

    std::cout << "==== End of Map ====" << std::endl;
}


void PrintModuleFunctionMap(ModuleFunctionMap &ModuleFunctionMap) {
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

void PrintStaticFunctionPointerMap(StaticFunctionPointerMap &StaticFPMap) {
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
