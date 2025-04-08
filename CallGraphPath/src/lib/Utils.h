#pragma once

#include "CallGraphPass.h"

void PrintModuleFunctionMap(const ModuleFunctionMap &ModuleFunctionMap, const std::string &ModName);
void PrintFunctionPointerSettings(const FunctionPointerSettings &FunctionPointerSettings);
void PrintFunctionPointerCallMap(const FunctionPointerCallMap &CallMap);