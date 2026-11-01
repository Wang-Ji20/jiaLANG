//===- Backend.h - Optimizing and emitting LLVM modules ---------*- C++ -*-===//
//
// A thin layer over LLVM's target machinery. Today LLVM is our whole back
// end; docs/roadmap.md describes how our own will replace it piece by piece.
//
//===----------------------------------------------------------------------===//
#pragma once

#include <expected>
#include <memory>
#include <string>

namespace llvm {
class Module;
class TargetMachine;
} // namespace llvm

namespace jia::driver {

enum class OutputKind { Object, Assembly, LLVMText, Bitcode };

/// Registers LLVM's targets. Call once, before createTargetMachine.
void initializeTargets();

/// A target machine for \p triple (empty: the host) at \p optLevel 0-3.
[[nodiscard]] std::expected<std::unique_ptr<llvm::TargetMachine>, std::string>
createTargetMachine(const std::string &triple, unsigned optLevel);

/// Sets the module's triple and data layout from \p machine.
void configureModule(llvm::Module &module, const llvm::TargetMachine &machine);

/// Runs LLVM's standard pipeline for \p optLevel, the same one clang uses.
void optimize(llvm::Module &module, llvm::TargetMachine &machine,
              unsigned optLevel);

/// Writes \p module to \p path ('-' for stdout) in the given form.
[[nodiscard]] std::expected<void, std::string>
emit(llvm::Module &module, llvm::TargetMachine &machine, OutputKind kind,
     const std::string &path);

} // namespace jia::driver
