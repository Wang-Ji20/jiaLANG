//===- Parser.h - Tokens to a syntax tree -----------------------*- C++ -*-===//
#pragma once

#include "jia/AST/AST.h"
#include "jia/Basic/Diagnostic.h"
#include "jia/Lex/Token.h"

#include <span>

namespace jia {

/// Builds a syntax tree from \p tokens, which must end with Eof. The tree
/// has no types on expressions yet (except explicit casts); run analyze()
/// on it next.
///
/// Pure: the result depends only on \p tokens. On errors, the parser skips
/// to the next statement or declaration and keeps going, so one run
/// reports many errors.
[[nodiscard]] Result<TranslationUnit> parse(std::span<const Token> tokens);

} // namespace jia
