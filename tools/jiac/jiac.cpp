//===- jiac.cpp - The jiaLang compiler executable -------------------------===//
#include "jia/Driver/Driver.h"

#include <cstdio>
#include <string_view>
#include <vector>

int main(int argc, char **argv) {
  const std::vector<std::string_view> args(argv + 1, argv + argc);
  const auto opts = jia::driver::parseArguments(args);
  if (!opts) {
    std::fprintf(stderr, "jiac: error: %s\n", opts.error().c_str());
    return 1;
  }
  return jia::driver::run(*opts);
}
