//===- ConstEval.h - Constant folding ---------------------------*- C++ -*-===//
#pragma once

#include "jia/AST/AST.h"

#include <cstdint>
#include <optional>
#include <variant>

namespace jia {

/// An integer or floating constant.
using ConstValue = std::variant<std::int64_t, double>;

/// Evaluates \p expr at compile time if it is built only from literals,
/// arithmetic, comparisons, and casts between arithmetic types. Returns
/// nullopt otherwise, or if evaluation would divide by zero.
///
/// Works both before Sema (when types are unknown, arithmetic is done in
/// 64 bits) and after it (when each result is narrowed to its type, so
/// (char)300 is 44).
[[nodiscard]] std::optional<ConstValue> evaluateConstant(const Expr &expr);

/// The constant as an integer, if it is one.
[[nodiscard]] std::optional<std::int64_t> evaluateInteger(const Expr &expr);

} // namespace jia
