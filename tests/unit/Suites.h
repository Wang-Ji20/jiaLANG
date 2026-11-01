//===- Suites.h - Every unit test suite, declared once ----------*- C++ -*-===//
#pragma once

#include "Test.h"

namespace jia::test {

extern const TestSuite basicSuite;
extern const TestSuite lexerSuite;
extern const TestSuite astSuite;
extern const TestSuite parserSuite;
extern const TestSuite semaSuite;
extern const TestSuite codegenSuite;

} // namespace jia::test
