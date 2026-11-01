//===- Backend.cpp - Optimizing and emitting LLVM modules -----------------===//
#include "jia/Driver/Backend.h"

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"

#include <format>

namespace jia::driver {
namespace {

llvm::OptimizationLevel passLevel(unsigned level) {
  switch (level) {
  case 0: return llvm::OptimizationLevel::O0;
  case 1: return llvm::OptimizationLevel::O1;
  case 2: return llvm::OptimizationLevel::O2;
  default: return llvm::OptimizationLevel::O3;
  }
}

llvm::CodeGenOptLevel codegenLevel(unsigned level) {
  switch (level) {
  case 0: return llvm::CodeGenOptLevel::None;
  case 1: return llvm::CodeGenOptLevel::Less;
  case 2: return llvm::CodeGenOptLevel::Default;
  default: return llvm::CodeGenOptLevel::Aggressive;
  }
}

} // namespace

void initializeTargets() {
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();
}

std::expected<std::unique_ptr<llvm::TargetMachine>, std::string>
createTargetMachine(const std::string &tripleName, unsigned optLevel) {
  const llvm::Triple triple(tripleName.empty() ? llvm::sys::getDefaultTargetTriple()
                                               : tripleName);
  std::string error;
  const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple, error);
  if (!target)
    return std::unexpected(error);
  // Position-independent code, so objects link into the PIE executables
  // that modern Linux distributions build by default.
  std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(
      triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::PIC_,
      std::nullopt, codegenLevel(optLevel)));
  if (!machine)
    return std::unexpected(std::format("cannot create a target machine for '{}'",
                                       triple.str()));
  return machine;
}

void configureModule(llvm::Module &module, const llvm::TargetMachine &machine) {
  module.setTargetTriple(machine.getTargetTriple());
  module.setDataLayout(machine.createDataLayout());
}

void optimize(llvm::Module &module, llvm::TargetMachine &machine,
              unsigned optLevel) {
  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;
  llvm::PassBuilder builder(&machine);
  builder.registerModuleAnalyses(mam);
  builder.registerCGSCCAnalyses(cgam);
  builder.registerFunctionAnalyses(fam);
  builder.registerLoopAnalyses(lam);
  builder.crossRegisterProxies(lam, fam, cgam, mam);
  llvm::ModulePassManager passes =
      optLevel == 0 ? builder.buildO0DefaultPipeline(llvm::OptimizationLevel::O0)
                    : builder.buildPerModuleDefaultPipeline(passLevel(optLevel));
  passes.run(module, mam);
}

std::expected<void, std::string> emit(llvm::Module &module,
                                      llvm::TargetMachine &machine,
                                      OutputKind kind, const std::string &path) {
  std::error_code ec;
  const auto flags = kind == OutputKind::LLVMText || kind == OutputKind::Assembly
                         ? llvm::sys::fs::OF_Text
                         : llvm::sys::fs::OF_None;
  llvm::raw_fd_ostream out(path, ec, flags);
  if (ec)
    return std::unexpected(std::format("cannot open '{}': {}", path, ec.message()));

  switch (kind) {
  case OutputKind::LLVMText:
    module.print(out, nullptr);
    break;
  case OutputKind::Bitcode:
    llvm::WriteBitcodeToFile(module, out);
    break;
  case OutputKind::Object:
  case OutputKind::Assembly: {
    llvm::legacy::PassManager passes;
    const auto fileType = kind == OutputKind::Object
                              ? llvm::CodeGenFileType::ObjectFile
                              : llvm::CodeGenFileType::AssemblyFile;
    if (machine.addPassesToEmitFile(passes, out, nullptr, fileType))
      return std::unexpected("the target cannot emit this kind of file");
    passes.run(module);
    break;
  }
  }
  out.flush();
  if (out.has_error())
    return std::unexpected(std::format("error writing '{}'", path));
  return {};
}

} // namespace jia::driver
