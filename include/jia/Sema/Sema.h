//===- Sema.h - Semantic analysis -------------------------------*- C++ -*-===//
#pragma once

#include "jia/AST/AST.h"
#include "jia/Basic/Diagnostic.h"

namespace jia {

/// Checks that a parsed program means something, and records what it
/// means. Consumes the parser's tree and returns it with:
///
///  - every expression's type and value category set;
///  - every name resolved to the SymbolId of its declaration;
///  - implicit conversions inserted as Cast nodes (isImplicit = true);
///  - a[i] rewritten to *(a + i), sizeof folded to an integer literal,
///    and pointer + integer reordered so the pointer is on the left;
///  - incomplete array types completed from their initializers.
///
/// Code generation relies on all of the above and checks none of it.
[[nodiscard]] Result<TranslationUnit> analyze(TranslationUnit tu);

} // namespace jia
