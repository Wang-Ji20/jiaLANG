//===- Test.h - A tiny macro-free unit test framework -----------*- C++ -*-===//
//
// Each test is a function taking a Tester. Checks record failures with the
// caller's file and line (via std::source_location) and keep going, so one
// run reports every broken check.
//
//   const TestSuite suite{"lexer", {
//       {"lexes an identifier", [](Tester &t) {
//          t.expectEq(kinds("abc"), "Identifier Eof");
//        }},
//   }};
//
//===----------------------------------------------------------------------===//
#pragma once

#include <concepts>
#include <format>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace jia::test {

/// Whether std::format can print a T. (std::formattable is missing from
/// the libstdc++ in GCC 14, which Ubuntu 24.04 ships.)
template <class T>
concept Printable = std::is_default_constructible_v<std::formatter<T, char>>;

class Tester {
public:
  void expect(bool condition, std::string_view what = "expectation failed",
              std::source_location loc = std::source_location::current()) {
    if (!condition)
      fail(std::string(what), loc);
  }

  template <class A, class B>
  void expectEq(const A &actual, const B &expected,
                std::source_location loc = std::source_location::current()) {
    if (actual == expected)
      return;
    if constexpr (Printable<A> && Printable<B>)
      fail(std::format("expected '{}'\n      actual   '{}'", expected, actual),
           loc);
    else
      fail("values differ", loc);
  }

  [[nodiscard]] int failures() const { return failures_; }

private:
  void fail(const std::string &message, std::source_location loc);
  int failures_ = 0;
};

struct TestCase {
  std::string_view name;
  void (*body)(Tester &);
};

struct TestSuite {
  std::string_view name;
  std::vector<TestCase> cases;
};

} // namespace jia::test
