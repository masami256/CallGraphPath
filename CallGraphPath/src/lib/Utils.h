#pragma once

#include "CallGraphPass.h"

void PrintModuleFunctionMap(const ModuleFunctionMap &ModuleFunctionMap, const std::string &ModName);
void PrintFunctionPointerSettings(const FunctionPointerSettings &FunctionPointerSettings);
void PrintFunctionPointerCallMap(const FunctionPointerCallMap &CallMap);
void PrintFunctionPointerUseMap(const FunctionPointerUseMap &UseMap);
void PrintCallGraph(const ModuleCallGraph &CallGraph);