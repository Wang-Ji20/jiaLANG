//===- ASTDumper.h - Print a tree as text -----------------------*- C++ -*-===//
#pragma once

#include "jia/AST/AST.h"

#include <string>
#include <string_view>

namespace jia {

/// Renders \p tu as an indented outline, one node per line, with types and
/// value categories when Sema has set them. Used by `jiac --dump-ast` and
/// by tests.
[[nodiscard]] std::string dumpAST(const TranslationUnit &tu);

[[nodiscard]] std::string_view name(UnaryOp op);
[[nodiscard]] std::string_view name(BinaryOp op);
[[nodiscard]] std::string_view name(CastKind kind);

} // namespace jia
