#include "Utils.h"
#include <iostream>

void PrintModuleFunctionMap(const ModuleFunctionMap &ModuleFunctionMap, const std::string &ModName) {
    // Debugging log to confirm the collected function prototypes for the specific module
    if (ModuleFunctionMap.find(ModName) != ModuleFunctionMap.end()) {
        for (const auto &funcEntry : ModuleFunctionMap.at(ModName)) {
            errs() << "[debug] Collected function prototypes for module: " << ModName << "\n";
            errs() << "Function: " << funcEntry.first << "\n";
            for (const auto &proto : funcEntry.second) {
                errs() << "  Return Type: " << std::get<0>(proto) << "\n";
                errs() << "  Arguments: ";
                for (const auto &arg : std::get<1>(proto)) {
                    errs() << arg << " ";
                }
                errs() << "\n  Line: " << std::get<2>(proto) << "\n";
            }
        }
    } else {
        errs() << "[debug] No function prototypes found for module: " << ModName << "\n";
    }
}

// // Function to print the collected function pointer settings for a given module
// Example of logging the contents of FunctionPointerSettings
void PrintFunctionPointerSettings(const FunctionPointerSettings &settings) {
    errs() << "==== Dump FunctionPointerSettings data ====\n";
    for (const auto &entry : settings) {
        const std::string &key = entry.first;  // Key is the module name + line number
        const std::vector<FunctionPointerSettingInfo> &functionSettings = entry.second;

        errs() << "[debug] Function pointer settings for " << key << ":\n";
        
        // Iterate through each setting in the vector
        for (const auto &setting : functionSettings) {
            errs() << "  Function pointer variable: " << setting.SetterName << "\n";
            errs() << "  Struct type (if applicable): " << setting.StructTypeName << "\n";
            errs() << "  Function name: " << setting.FuncName << "\n";
            errs() << "  Line: " << setting.Line << "\n";
            errs() << "  Offset: " << setting.Offset << "\n";
        }
    }
    errs() << "==== Dump FunctionPointerSettings data end ====\n";
}
