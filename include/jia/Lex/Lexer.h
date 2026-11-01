//===- Lexer.h - Source text to tokens --------------------------*- C++ -*-===//
#pragma once

#include "jia/Basic/Diagnostic.h"
#include "jia/Lex/Token.h"

#include <string_view>
#include <vector>

namespace jia {

/// Splits \p source into tokens, skipping whitespace and comments. The last
/// token is always Eof. Reports every malformed token, not just the first.
///
/// Pure: the result depends only on \p source. The tokens borrow from
/// \p source, so keep it alive while you use them.
[[nodiscard]] Result<std::vector<Token>> lex(std::string_view source);

} // namespace jia
