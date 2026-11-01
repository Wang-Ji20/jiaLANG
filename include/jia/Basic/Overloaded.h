//===- Overloaded.h - Pattern matching on std::variant ----------*- C++ -*-===//
//
// Overloaded turns a set of lambdas into one visitor, so std::visit reads
// like an OCaml match:
//
//   std::visit(Overloaded{
//       [](const IntLiteral &lit) { ... },
//       [](const Binary &bin) { ... },
//   }, expr.node);
//
//===----------------------------------------------------------------------===//
#pragma once

namespace jia {

template <class... Fs> struct Overloaded : Fs... {
  using Fs::operator()...;
};

} // namespace jia
