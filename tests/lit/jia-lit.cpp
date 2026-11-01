//===- jia-lit.cpp - Run file-based compiler tests ------------------------===//
//
// A small, self-contained stand-in for LLVM's lit and FileCheck. Each test
// is a .jia file whose comments say how to run it and what to expect:
//
//   // RUN: %jiac %s -o %t && %t        command that must succeed
//   // RUN-FAIL: %jiac %s               command that must fail
//   // CHECK: hello                     output contains "hello" ...
//   // CHECK-NEXT: world                ... and "world" on the next line
//   // CHECK-NOT: warning               ... and no "warning" before the
//                                        next CHECK (or the end)
//
// Commands run with /bin/sh; stdout and stderr of all RUN lines are joined
// into one output that the CHECK lines are matched against, in order.
//
// Substitutions: %jiac (the compiler), %cc (the C compiler), %s (the test
// file), %S (its directory), %t (a scratch path unique to the test).
//
// Usage: jia-lit --jiac <path> [--cc <cc>] [--out <dir>] [-v] <file|dir>...
//
//===----------------------------------------------------------------------===//
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Config {
  std::string jiac;
  std::string cc = "cc";
  fs::path outDir = fs::temp_directory_path() / "jia-lit";
  bool verbose = false;
  std::vector<fs::path> inputs;
};

enum class CheckKind { Check, Next, Not };

struct Check {
  CheckKind kind;
  std::string text;
  int line; // in the test file, for error messages
};

struct RunLine {
  std::string command;
  bool expectFailure;
};

struct TestSpec {
  std::vector<RunLine> runs;
  std::vector<Check> checks;
};

std::string trim(std::string_view s) {
  const auto begin = s.find_first_not_of(" \t\r");
  if (begin == std::string_view::npos)
    return "";
  const auto end = s.find_last_not_of(" \t\r");
  return std::string(s.substr(begin, end - begin + 1));
}

std::string replaceAll(std::string s, std::string_view from, std::string_view to) {
  for (std::size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos;
       pos += to.size())
    s.replace(pos, from.size(), to);
  return s;
}

/// Reads the RUN and CHECK directives of one test file.
TestSpec readSpec(const fs::path &file) {
  TestSpec spec;
  std::ifstream in(file);
  std::string line;
  for (int number = 1; std::getline(in, line); ++number) {
    const auto slash = line.find("//");
    if (slash == std::string::npos)
      continue;
    const std::string comment = trim(std::string_view(line).substr(slash + 2));
    const auto directive = [&](std::string_view name) -> std::optional<std::string> {
      if (!comment.starts_with(name))
        return std::nullopt;
      return trim(std::string_view(comment).substr(name.size()));
    };
    if (auto failing = directive("RUN-FAIL:"))
      spec.runs.push_back({*failing, true});
    else if (auto passing = directive("RUN:"))
      spec.runs.push_back({*passing, false});
    else if (auto nextLine = directive("CHECK-NEXT:"))
      spec.checks.push_back({CheckKind::Next, *nextLine, number});
    else if (auto absent = directive("CHECK-NOT:"))
      spec.checks.push_back({CheckKind::Not, *absent, number});
    else if (auto present = directive("CHECK:"))
      spec.checks.push_back({CheckKind::Check, *present, number});
  }
  return spec;
}

/// Runs \p command through the shell; returns its exit status and output.
std::pair<int, std::string> shell(const std::string &command) {
  const std::string full = std::format("{{ {}\n}} 2>&1", command);
  FILE *pipe = popen(full.c_str(), "r");
  if (!pipe)
    return {-1, "jia-lit: cannot start /bin/sh\n"};
  std::string output;
  char buffer[4096];
  while (const std::size_t n = std::fread(buffer, 1, sizeof buffer, pipe))
    output.append(buffer, n);
  const int status = pclose(pipe);
  return {WIFEXITED(status) ? WEXITSTATUS(status) : 128, output};
}

/// Matches \p checks against \p output in order. Returns an error message,
/// or nothing when every check holds.
std::optional<std::string> match(const std::vector<Check> &checks,
                                 const std::string &output) {
  std::vector<std::string> lines;
  std::istringstream stream(output);
  for (std::string line; std::getline(stream, line);)
    lines.push_back(line);

  std::size_t next = 0;          // first line not yet consumed
  std::optional<std::size_t> last; // line of the previous positive match
  std::vector<const Check *> pendingNots;

  const auto checkNots = [&](std::size_t from, std::size_t to) -> std::optional<std::string> {
    for (const Check *no : pendingNots)
      for (std::size_t i = from; i < to && i < lines.size(); ++i)
        if (lines[i].find(no->text) != std::string::npos)
          return std::format("line {}: CHECK-NOT: found '{}' in output line {}:\n  {}",
                             no->line, no->text, i + 1, lines[i]);
    pendingNots.clear();
    return std::nullopt;
  };

  for (const Check &check : checks) {
    if (check.kind == CheckKind::Not) {
      pendingNots.push_back(&check);
      continue;
    }
    std::optional<std::size_t> found;
    if (check.kind == CheckKind::Next) {
      const std::size_t want = last ? *last + 1 : 0;
      if (want < lines.size() && lines[want].find(check.text) != std::string::npos)
        found = want;
      else
        return std::format("line {}: CHECK-NEXT: expected '{}' on output line {}, got:\n  {}",
                           check.line, check.text, want + 1,
                           want < lines.size() ? lines[want] : "<end of output>");
    } else {
      for (std::size_t i = next; i < lines.size() && !found; ++i)
        if (lines[i].find(check.text) != std::string::npos)
          found = i;
      if (!found)
        return std::format("line {}: CHECK: '{}' not found in output", check.line,
                           check.text);
    }
    if (auto err = checkNots(next, *found))
      return err;
    last = found;
    next = *found + 1;
  }
  return checkNots(next, lines.size());
}

bool runTest(const Config &config, const fs::path &file, std::size_t index) {
  const TestSpec spec = readSpec(file);
  const fs::path scratch = config.outDir / std::format("t{}-{}", index, file.stem().string());
  std::string output;
  std::optional<std::string> failure;
  if (spec.runs.empty())
    failure = "no RUN line";

  for (const RunLine &run : spec.runs) {
    std::string cmd = run.command;
    cmd = replaceAll(cmd, "%jiac", config.jiac);
    cmd = replaceAll(cmd, "%cc", config.cc);
    cmd = replaceAll(cmd, "%s", file.string());
    cmd = replaceAll(cmd, "%S", file.parent_path().string());
    cmd = replaceAll(cmd, "%t", scratch.string());
    const auto [status, out] = shell(cmd);
    output += out;
    if ((status == 0) == run.expectFailure) {
      failure = std::format("command {} (exit {}):\n  {}",
                            run.expectFailure ? "unexpectedly succeeded" : "failed",
                            status, cmd);
      break;
    }
  }
  if (!failure)
    failure = match(spec.checks, output);

  std::printf("%s %s\n", failure ? "FAIL" : "PASS", file.string().c_str());
  if (failure || config.verbose) {
    if (failure)
      std::printf("  %s\n", failure->c_str());
    std::printf("  --- output ---\n%s  --------------\n", output.c_str());
  }
  return !failure;
}

std::optional<Config> parseArgs(int argc, char **argv) {
  Config config;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto value = [&]() -> std::optional<std::string> {
      if (i + 1 >= argc)
        return std::nullopt;
      return std::string(argv[++i]);
    };
    if (arg == "--jiac") {
      if (auto v = value()) config.jiac = *v; else return std::nullopt;
    } else if (arg == "--cc") {
      if (auto v = value()) config.cc = *v; else return std::nullopt;
    } else if (arg == "--out") {
      if (auto v = value()) config.outDir = *v; else return std::nullopt;
    } else if (arg == "-v") {
      config.verbose = true;
    } else {
      config.inputs.emplace_back(arg);
    }
  }
  if (config.jiac.empty() || config.inputs.empty())
    return std::nullopt;
  return config;
}

} // namespace

int main(int argc, char **argv) {
  const auto config = parseArgs(argc, argv);
  if (!config) {
    std::fprintf(stderr,
                 "usage: jia-lit --jiac <path> [--cc <cc>] [--out <dir>] [-v] <file|dir>...\n");
    return 2;
  }

  // Directories are searched recursively for .jia files, skipping
  // "Inputs" directories, which hold helper files rather than tests.
  std::vector<fs::path> tests;
  for (const fs::path &input : config->inputs) {
    if (!fs::is_directory(input)) {
      tests.push_back(input);
      continue;
    }
    for (auto it = fs::recursive_directory_iterator(input);
         it != fs::recursive_directory_iterator(); ++it) {
      if (it->is_directory() && it->path().filename() == "Inputs")
        it.disable_recursion_pending();
      else if (it->path().extension() == ".jia")
        tests.push_back(it->path());
    }
  }
  std::sort(tests.begin(), tests.end());
  fs::create_directories(config->outDir);

  std::size_t failed = 0;
  for (std::size_t i = 0; i < tests.size(); ++i)
    failed += runTest(*config, tests[i], i) ? 0 : 1;
  std::printf("%zu tests, %zu failed\n", tests.size(), failed);
  return failed == 0 ? 0 : 1;
}
