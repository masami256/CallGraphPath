
#include "IRDumper.h"
#include <llvm/Support/Path.h>
#include <llvm/Support/FileSystem.h>

using namespace llvm;

void saveModule(Module &M, Twine filename)
{
    int bc_fd;
    StringRef FN = filename.getSingleStringRef();
    StringRef Path = sys::path::parent_path(FN);
    //std::cout << "Path: " << Path.str() << "\n";
    std::string OutputDir = (Twine("bcfiles") + "/" + Path).str();

    if (!sys::fs::exists(OutputDir)) {
        sys::fs::create_directories(OutputDir);
        std::cout << "Created directory: " << OutputDir << "\n";
    }

    std::string OriginalFileName = sys::path::filename(FN).str();
    SmallString<1024> OutputFile(OutputDir + "/" + OriginalFileName);

    sys::path::replace_extension(OutputFile, ".bc");

    sys::fs::openFileForWrite(OutputFile, bc_fd);
    raw_fd_ostream bc_file(bc_fd, true, true);

    WriteBitcodeToFile(M, bc_file);
}

struct IRDumperPass : public PassInfoMixin<IRDumperPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
		std::cout << "IRDumperPass in function: " << M.getName().str() << std::endl;
        saveModule(M, M.getName());
        return PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
    return {.APIVersion = LLVM_PLUGIN_API_VERSION,
            .PluginName = "IRDumper",
            .PluginVersion = LLVM_VERSION_STRING,
            .RegisterPassBuilderCallbacks = [](PassBuilder& PB) {
                PB.registerOptimizerEarlyEPCallback(
                    [](ModulePassManager& PM, OptimizationLevel /* Level */) {
                        PM.addPass(IRDumperPass{});
                    });
            }};
}
