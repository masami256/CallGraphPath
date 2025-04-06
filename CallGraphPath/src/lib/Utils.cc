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
            const std::set<std::string> &entries = funcEntry.second;

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

void PrintCallGraph(DirectCallMap &DirectCallMap, IndirectCallMap &IndirectCallMap, bool verbose) {
    std::cout << "==== Direct Call Graph ====" << std::endl;

    for (const auto &modEntry : DirectCallMap) {
        const std::string &modName = modEntry.first;
        const auto &funcMap = modEntry.second;

        for (const auto &funcEntry : funcMap) {
            const std::string &caller = funcEntry.first;
            const auto &callees = funcEntry.second;

            for (const std::string &entry : callees) {
                // entry format: callee:line
                size_t sep = entry.find_last_of(':');
                std::string callee = entry.substr(0, sep);
                std::string line = entry.substr(sep + 1);

                std::cout << modName << ": " << caller << " -> " << callee << " [line " << line << "]" << std::endl;
            }
        }
    }

    std::cout << std::endl << "==== Indirect Call Graph ====" << std::endl;

    for (const auto &modEntry : IndirectCallMap) {
        const std::string &modName = modEntry.first;
        const auto &funcMap = modEntry.second;

        for (const auto &funcEntry : funcMap) {
            const std::string &caller = funcEntry.first;
            const auto &entries = funcEntry.second;

            for (const std::string &entry : entries) {
                // entry format: calledExpr:line
                size_t sep = entry.find_last_of(':');
                std::string expr = entry.substr(0, sep);
                std::string line = entry.substr(sep + 1);

                std::cout << modName << ": " << caller << " -> [indirect] [line " << line << "]" << std::endl;

                if (verbose) {
                    std::cout << "    IR: " << expr << std::endl;
                }
            }
        }
    }

    std::cout << "==== End of Call Graph ====" << std::endl;
}

void PrintResolvedIndirectCalls(IndirectCallCandidates &IndirectCallCandidates) {
    std::cout << "==== Resolved Indirect Calls ====" << std::endl;
    for (const auto &modEntry : IndirectCallCandidates) {
        const std::string &mod = modEntry.first;
        const auto &funcMap = modEntry.second;

        for (const auto &fentry : funcMap) {
            const std::string &caller = fentry.first;
            const auto &lineMap = fentry.second;

            for (const auto &lineEntry : lineMap) {
                unsigned line = lineEntry.first;
                const auto &targets = lineEntry.second;

                std::cout << mod << ": " << caller << " [line " << line << "] -> ";

                if (targets.empty()) {
                    std::cout << "(unknown)";
                } else {
                    bool first = true;
                    for (const auto &target : targets) {
                        if (!first) std::cout << " ";
                        std::cout << target;
                        first = false;
                    }
                }

                std::cout << std::endl;
            }
        }
    }
    std::cout << "==== End of Resolved Indirect Calls ====" << std::endl;
}
