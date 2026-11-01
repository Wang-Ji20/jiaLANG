//===- Diagnostic.cpp - Rendering diagnostics -----------------------------===//
#include "jia/Basic/Diagnostic.h"

#include <format>

namespace jia {
namespace {

std::string_view severityName(Severity severity) {
  switch (severity) {
  case Severity::Error:
    return "error";
  case Severity::Warning:
    return "warning";
  case Severity::Note:
    return "note";
  }
  return "error";
}

/// Returns the text of 1-based line \p line, without its newline.
std::string_view lineText(std::string_view source, std::uint32_t line) {
  std::size_t begin = 0;
  for (std::uint32_t current = 1; current < line; ++current) {
    begin = source.find('\n', begin);
    if (begin == std::string_view::npos)
      return {};
    ++begin;
  }
  const std::size_t end = source.find('\n', begin);
  return source.substr(begin,
                       end == std::string_view::npos ? end : end - begin);
}

} // namespace

std::string render(const Diagnostic &diag, std::string_view fileName,
                   std::string_view source) {
  if (!diag.loc.isValid())
    return std::format("{}: {}: {}\n", fileName, severityName(diag.severity),
                       diag.message);

  std::string out =
      std::format("{}:{}:{}: {}: {}\n", fileName, diag.loc.line,
                  diag.loc.column, severityName(diag.severity), diag.message);
  const std::string_view text = lineText(source, diag.loc.line);
  if (text.empty())
    return out;

  const std::string gutter = std::format("{:>5}", diag.loc.line);
  out += std::format("{} | {}\n", gutter, text);
  // Keep tabs in the caret line so the caret lines up with the source.
  std::string pad;
  for (std::uint32_t i = 1; i < diag.loc.column && i <= text.size(); ++i)
    pad += text[i - 1] == '\t' ? '\t' : ' ';
  out += std::format("{} | {}^\n", std::string(gutter.size(), ' '), pad);
  return out;
}

} // namespace jia
