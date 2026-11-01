//===- ConstEval.cpp - Constant folding -----------------------------------===//
#include "jia/AST/ConstEval.h"

#include "jia/Basic/Overloaded.h"

#include <limits>

namespace jia {
namespace {

using Opt = std::optional<ConstValue>;

double asDouble(ConstValue v) {
  return std::visit([](auto x) { return static_cast<double>(x); }, v);
}

/// Narrows \p value to what \p type can hold. Integer narrowing wraps, as
/// on every target we support.
Opt fitTo(ConstValue value, const Type *type) {
  if (!type)
    return value;
  switch (type->kind()) {
  case TypeKind::Char:
  case TypeKind::Int: {
    const auto *i = std::get_if<std::int64_t>(&value);
    if (!i) {
      const double d = std::get<double>(value);
      if (!(d > -9.3e18 && d < 9.3e18))
        return std::nullopt;
      value = static_cast<std::int64_t>(d);
      i = std::get_if<std::int64_t>(&value);
    }
    if (type->is(TypeKind::Char))
      return static_cast<std::int64_t>(static_cast<std::int8_t>(*i));
    return static_cast<std::int64_t>(static_cast<std::int32_t>(*i));
  }
  case TypeKind::Float:
    return static_cast<double>(static_cast<float>(asDouble(value)));
  default:
    return std::nullopt;
  }
}

Opt foldBinary(BinaryOp op, ConstValue l, ConstValue r) {
  const bool isInt = std::holds_alternative<std::int64_t>(l) &&
                     std::holds_alternative<std::int64_t>(r);
  if (op == BinaryOp::LogAnd || op == BinaryOp::LogOr) {
    const bool lb = asDouble(l) != 0, rb = asDouble(r) != 0;
    return std::int64_t{op == BinaryOp::LogAnd ? (lb && rb) : (lb || rb)};
  }
  if (isInt) {
    // Wrap instead of overflowing, which would be undefined behaviour.
    const auto a = static_cast<std::uint64_t>(std::get<std::int64_t>(l));
    const auto b = static_cast<std::uint64_t>(std::get<std::int64_t>(r));
    const auto sa = std::get<std::int64_t>(l), sb = std::get<std::int64_t>(r);
    switch (op) {
    case BinaryOp::Add: return static_cast<std::int64_t>(a + b);
    case BinaryOp::Sub: return static_cast<std::int64_t>(a - b);
    case BinaryOp::Mul: return static_cast<std::int64_t>(a * b);
    case BinaryOp::Div:
    case BinaryOp::Rem:
      if (sb == 0 ||
          (sa == std::numeric_limits<std::int64_t>::min() && sb == -1))
        return std::nullopt;
      return op == BinaryOp::Div ? sa / sb : sa % sb;
    case BinaryOp::Lt: return std::int64_t{sa < sb};
    case BinaryOp::Gt: return std::int64_t{sa > sb};
    case BinaryOp::Le: return std::int64_t{sa <= sb};
    case BinaryOp::Ge: return std::int64_t{sa >= sb};
    case BinaryOp::Eq: return std::int64_t{sa == sb};
    case BinaryOp::Ne: return std::int64_t{sa != sb};
    default: return std::nullopt;
    }
  }
  const double a = asDouble(l), b = asDouble(r);
  switch (op) {
  case BinaryOp::Add: return a + b;
  case BinaryOp::Sub: return a - b;
  case BinaryOp::Mul: return a * b;
  case BinaryOp::Div: return a / b;
  case BinaryOp::Lt: return std::int64_t{a < b};
  case BinaryOp::Gt: return std::int64_t{a > b};
  case BinaryOp::Le: return std::int64_t{a <= b};
  case BinaryOp::Ge: return std::int64_t{a >= b};
  case BinaryOp::Eq: return std::int64_t{a == b};
  case BinaryOp::Ne: return std::int64_t{a != b};
  default: return std::nullopt; // % on floats is not constant
  }
}

Opt fold(const Expr &expr) {
  return std::visit(
      Overloaded{
          [](const IntLiteral &lit) -> Opt { return lit.value; },
          [](const FloatLiteral &lit) -> Opt { return lit.value; },
          [&](const Unary &u) -> Opt {
            const Opt v = evaluateConstant(*u.operand);
            if (!v)
              return std::nullopt;
            switch (u.op) {
            case UnaryOp::Plus:
              return v;
            case UnaryOp::Minus:
              if (const auto *i = std::get_if<std::int64_t>(&*v))
                return static_cast<std::int64_t>(
                    0 - static_cast<std::uint64_t>(*i));
              return -std::get<double>(*v);
            case UnaryOp::Not:
              return std::int64_t{asDouble(*v) == 0};
            default:
              return std::nullopt;
            }
          },
          [](const Binary &b) -> Opt {
            const Opt l = evaluateConstant(*b.lhs);
            const Opt r = evaluateConstant(*b.rhs);
            if (!l || !r)
              return std::nullopt;
            return foldBinary(b.op, *l, *r);
          },
          [&](const Cast &c) -> Opt {
            const bool arithmetic =
                c.kind == CastKind::Unresolved || c.kind == CastKind::NoOp ||
                c.kind == CastKind::IntegralCast ||
                c.kind == CastKind::IntToFloat ||
                c.kind == CastKind::FloatToInt;
            if (!arithmetic)
              return std::nullopt;
            return evaluateConstant(*c.operand);
          },
          [](const SizeOfType &s) -> Opt {
            if (const auto size = s.operand->size())
              return static_cast<std::int64_t>(*size);
            return std::nullopt;
          },
          [](const auto &) -> Opt { return std::nullopt; },
      },
      expr.node);
}

} // namespace

std::optional<ConstValue> evaluateConstant(const Expr &expr) {
  const Opt value = fold(expr);
  if (!value)
    return std::nullopt;
  return fitTo(*value, expr.type);
}

std::optional<std::int64_t> evaluateInteger(const Expr &expr) {
  const Opt value = evaluateConstant(expr);
  if (!value)
    return std::nullopt;
  if (const auto *i = std::get_if<std::int64_t>(&*value))
    return *i;
  return std::nullopt;
}

} // namespace jia
