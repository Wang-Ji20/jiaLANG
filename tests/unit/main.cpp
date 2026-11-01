//===- main.cpp - Unit test runner ----------------------------------------===//
//
// Usage: jia-unittests [substring]
// Runs every test whose "suite/name" contains the substring.
//
//===----------------------------------------------------------------------===//
#include "Suites.h"

#include <cstdio>

namespace jia::test {

void Tester::fail(const std::string &message, std::source_location loc) {
  ++failures_;
  std::printf("    %s:%u: %s\n", loc.file_name(),
              static_cast<unsigned>(loc.line()), message.c_str());
}

} // namespace jia::test

int main(int argc, char **argv) {
  using namespace jia::test;
  const std::string_view filter = argc > 1 ? argv[1] : "";
  const TestSuite *suites[] = {&basicSuite};

  int ran = 0, failed = 0;
  for (const TestSuite *suite : suites) {
    for (const TestCase &test : suite->cases) {
      const std::string fullName = std::format("{}/{}", suite->name, test.name);
      if (fullName.find(filter) == std::string::npos)
        continue;
      Tester tester;
      test.body(tester);
      ++ran;
      if (tester.failures() != 0) {
        ++failed;
        std::printf("FAIL %s\n", fullName.c_str());
      }
    }
  }
  std::printf("%d tests, %d failed\n", ran, failed);
  return failed == 0 ? 0 : 1;
}
