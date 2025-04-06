#pragma once

#include "CallGraphPass.h"

void PrintFuncPtrArgumentMap(FunctionPtrArgMap &FuncPtrArgMap);
void PrintDynamicFunctionPointerMap(DynamicFunctionPointerMap &DynamicFPMap);
void PrintModuleFunctionMap(ModuleFunctionMap &ModuleFunctionMap);
void PrintStaticFunctionPointerMap(StaticFunctionPointerMap &StaticFPMap);
void PrintCallGraph(DirectCallMap &DirectCallMap, IndirectCallMap &IndirectCallMap, bool verbose = false);
void PrintResolvedIndirectCalls(const IndirectCallCandidates &IndirectCallCandidates);
void DumpEntireIndirectCallCandidates(const IndirectCallCandidates &Map);