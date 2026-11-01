//===- jia-bench.cpp - Compare jiac with the system C compiler ------------===//
//
// For each benchmark program (valid as both Jia and C), builds it with
// every configuration (jiac and cc, at -O0 and -O2), then measures:
//
//   - compile time: median wall time of building the executable;
//   - run time:     median wall time of running it;
//   - correctness:  every configuration must print the same output.
//
// It also generates a large synthetic source file and measures how fast
// each compiler gets through it (front end only, and a full -O0 compile).
//
// Usage: jia-bench --jiac <path> [--cc <cc>] [--runs N] [--out DIR]
//                  [--report FILE] [--no-throughput] <program.jia>...
//
// Prints a Markdown report; --report also writes it to a file. Exits with
// 1 if any configuration fails to build or prints different output.
//
//===----------------------------------------------------------------------===//
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Config {
  std::string jiac;
  std::string cc = "cc";
  int runs = 5;
  fs::path outDir = fs::temp_directory_path() / "jia-bench";
  std::optional<fs::path> report;
  bool throughput = true;
  std::vector<fs::path> programs;
};

/// One way of building a program.
struct Compiler {
  std::string name;  // shown in the report, e.g. "jiac -O2"
  std::string build; // command with {in} and {out} placeholders
  std::string check; // front-end-only command with {in}
};

std::vector<Compiler> compilers(const Config &config) {
  std::vector<Compiler> list;
  for (const char *level : {"-O0", "-O2"}) {
    list.push_back({std::format("jiac {}", level),
                    std::format("{} {} {{in}} -o {{out}}", config.jiac, level),
                    std::format("{} -fsyntax-only {{in}}", config.jiac)});
    list.push_back(
        {std::format("cc {}", level),
         std::format("{} -w -x c {} {{in}} -o {{out}}", config.cc, level),
         std::format("{} -w -x c -fsyntax-only {{in}}", config.cc)});
  }
  return list;
}

std::string substitute(std::string pattern, const std::string &in,
                       const std::string &out) {
  const auto put = [&](std::string_view key, const std::string &value) {
    for (std::size_t pos; (pos = pattern.find(key)) != std::string::npos;)
      pattern.replace(pos, key.size(), value);
  };
  put("{in}", in);
  put("{out}", out);
  return pattern;
}

/// Runs \p command through the shell and returns its wall time in
/// seconds, or nothing if it failed.
std::optional<double> timed(const std::string &command) {
  const auto start = std::chrono::steady_clock::now();
  const int status = std::system(command.c_str());
  const auto stop = std::chrono::steady_clock::now();
  if (status != 0)
    return std::nullopt;
  return std::chrono::duration<double>(stop - start).count();
}

/// The median of \p runs timings of \p command.
std::optional<double> median(const std::string &command, int runs) {
  std::vector<double> times;
  for (int i = 0; i < runs; ++i) {
    const auto t = timed(command);
    if (!t)
      return std::nullopt;
    times.push_back(*t);
  }
  std::sort(times.begin(), times.end());
  return times[times.size() / 2];
}

std::string readFile(const fs::path &path) {
  std::ifstream in(path);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string ms(std::optional<double> seconds) {
  return seconds ? std::format("{:.1f}", *seconds * 1000) : "FAIL";
}

/// A large program made of many small functions, to measure compile speed.
fs::path writeSynthetic(const fs::path &dir, int functions) {
  const fs::path path = dir / "synthetic.jia";
  std::ofstream out(path);
  out << "int printf(char *fmt, ...);\n";
  for (int i = 0; i < functions; ++i) {
    out << std::format("int f{0}(int n, int *a) {{\n"
                       "  int s = {0}, i = 0;\n"
                       "  while (i < n) {{\n"
                       "    if (a[i] % 3 == 0 && i > 2) s = s + a[i] * 2;\n"
                       "    else s = s - (a[i] + {0}) / 7;\n"
                       "    i = i + 1;\n"
                       "  }}\n"
                       "  return s;\n"
                       "}}\n",
                       i);
  }
  out << "int main() { int a[4] = {1, 2, 3, 4}; int s = 0;\n";
  for (int i = 0; i < functions; ++i)
    out << std::format("  s = s + f{}(4, a);\n", i);
  out << "  printf(\"%d\\n\", s);\n  return 0;\n}\n";
  return path;
}

std::optional<Config> parseArgs(int argc, char **argv) {
  Config config;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const bool hasValue = i + 1 < argc;
    if (arg == "--jiac" && hasValue)
      config.jiac = argv[++i];
    else if (arg == "--cc" && hasValue)
      config.cc = argv[++i];
    else if (arg == "--runs" && hasValue)
      config.runs = std::max(1, std::atoi(argv[++i]));
    else if (arg == "--out" && hasValue)
      config.outDir = argv[++i];
    else if (arg == "--report" && hasValue)
      config.report = argv[++i];
    else if (arg == "--no-throughput")
      config.throughput = false;
    else if (arg.starts_with("-"))
      return std::nullopt;
    else
      config.programs.emplace_back(arg);
  }
  if (config.jiac.empty() || config.programs.empty())
    return std::nullopt;
  return config;
}

} // namespace

int main(int argc, char **argv) {
  const auto config = parseArgs(argc, argv);
  if (!config) {
    std::fprintf(
        stderr,
        "usage: jia-bench --jiac <path> [--cc <cc>] [--runs N] "
        "[--out DIR] [--report FILE] [--no-throughput] <program.jia>...\n");
    return 2;
  }
  fs::create_directories(config->outDir);
  const std::vector<Compiler> list = compilers(*config);
  bool ok = true;

  std::string report =
      std::format("# Benchmark report\n\nMedian of {} runs, in milliseconds. "
                  "Compile = build the executable; run = execute it.\n\n",
                  config->runs);
  report += "| Program |";
  for (const Compiler &c : list)
    report += std::format(" {} compile | {} run |", c.name, c.name);
  report += " Output |\n|---|";
  for (std::size_t i = 0; i < list.size(); ++i)
    report += "---:|---:|";
  report += "---|\n";

  for (const fs::path &program : config->programs) {
    const std::string name = program.stem().string();
    std::fprintf(stderr, "benchmarking %s...\n", name.c_str());
    report += std::format("| {} |", name);
    std::optional<std::string> reference;
    bool same = true;
    for (std::size_t i = 0; i < list.size(); ++i) {
      const fs::path exe = config->outDir / std::format("{}-{}", name, i);
      const fs::path output =
          config->outDir / std::format("{}-{}.out", name, i);
      const auto compile =
          median(substitute(list[i].build, program.string(), exe.string()),
                 config->runs);
      std::optional<double> run;
      if (compile) {
        run = median(std::format("{} > {}", exe.string(), output.string()),
                     config->runs);
        const std::string printed = readFile(output);
        if (!reference)
          reference = printed;
        same &= printed == *reference;
      }
      ok &= compile && run;
      report += std::format(" {} | {} |", ms(compile), ms(run));
    }
    ok &= same;
    report += same ? " same |\n" : " **DIFFERS** |\n";
  }

  if (config->throughput) {
    const int functions = 2000;
    const fs::path synthetic = writeSynthetic(config->outDir, functions);
    const std::string text = readFile(synthetic);
    const auto lines = std::count(text.begin(), text.end(), '\n');
    report += std::format(
        "\n## Compile throughput\n\nA generated file of {} functions "
        "({} lines).\n\n| Compiler | Front end only | Full build |\n"
        "|---|---:|---:|\n",
        functions, lines);
    std::fprintf(stderr, "measuring throughput...\n");
    for (const Compiler &c : list) {
      if (c.name.ends_with("-O2"))
        continue; // front-end time does not depend on the level
      const fs::path exe = config->outDir / "synthetic";
      const auto check =
          median(substitute(c.check, synthetic.string(), ""), config->runs);
      const auto build = median(
          substitute(c.build, synthetic.string(), exe.string()), config->runs);
      ok &= check && build;
      report += std::format("| {} | {} | {} |\n", c.name, ms(check), ms(build));
    }
  }

  std::fputs(report.c_str(), stdout);
  if (config->report)
    std::ofstream(*config->report) << report;
  return ok ? 0 : 1;
}
