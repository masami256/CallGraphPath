#include "llvm/Support/CommandLine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"

#include "Analyzer.h"
#include "CallGraphPass.h"

#include <chrono>
#include <iostream>

using namespace llvm;
cl::list<std::string> InputFilenames(
    cl::Positional, cl::OneOrMore, cl::desc("<input bitcode files>"));

ModuleList Modules;

int main(int argc, char **argv) 
{
	auto start = std::chrono::system_clock::now();

    llvm::cl::ParseCommandLineOptions(argc, argv, "global analysis\n");

    std::cout << "Total " << InputFilenames.size() << " file(s)" << std::endl;

    SMDiagnostic Err;
    for (unsigned i = 0; i < InputFilenames.size(); ++i) {
        std::cout << "File " << i + 1 << ": " << InputFilenames[i] << std::endl;

		llvm::LLVMContext *context = new llvm::LLVMContext();
        std::unique_ptr<Module> M = parseIRFile(InputFilenames[i], Err, *context);
        if (!M) {
            std::cerr << "Error reading file: " << InputFilenames[i] << std::endl;
            continue;
        }

        Module *Module = M.release();
        StringRef ModuleName = StringRef(strdup(InputFilenames[i].data()));
        Modules.push_back(std::make_pair(Module, ModuleName));
    }

    CallGraphPass CGPass("CallGraphPass");
	CGPass.run(Modules);

	return 0;
}