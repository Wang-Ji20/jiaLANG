//===- CodeGen.h - Typed tree to LLVM IR ------------------------*- C++ -*-===//
#pragma once

#include "jia/AST/AST.h"
#include "jia/Basic/Diagnostic.h"

#include <memory>
#include <string_view>

namespace llvm {
class LLVMContext;
class Module;
} // namespace llvm

namespace jia {

/// Translates a tree produced by analyze() into an LLVM module.
///
/// The tree must have passed Sema; code generation trusts every type and
/// conversion in it. The only failure is an internal one: if the LLVM
/// verifier rejects the module, the diagnostic describes a compiler bug.
///
/// The module borrows \p context, which must outlive it. The caller sets
/// the target triple and data layout before optimizing or emitting.
[[nodiscard]] Result<std::unique_ptr<llvm::Module>>
generateModule(const TranslationUnit &tu, llvm::LLVMContext &context,
               std::string_view moduleName);

} // namespace jia
