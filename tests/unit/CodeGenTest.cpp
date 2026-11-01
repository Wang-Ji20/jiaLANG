#include "jia/CodeGen/CodeGen.h"
#include "Suites.h"
#include "jia/Lex/Lexer.h"
#include "jia/Parse/Parser.h"
#include "jia/Sema/Sema.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

namespace jia::test {
namespace {

/// The textual IR for \p source, or "error: <first>".
std::string ir(std::string_view source) {
  llvm::LLVMContext ctx;
  auto module = lex(source)
                    .and_then([](std::vector<Token> t) { return parse(t); })
                    .and_then(analyze)
                    .and_then([&](const TranslationUnit &tu) {
                      return generateModule(tu, ctx, "test");
                    });
  if (!module)
    return "error: " + module.error().front().message;
  std::string out;
  llvm::raw_string_ostream os(out);
  (*module)->print(os, nullptr);
  return out;
}

bool contains(const std::string &haystack, std::string_view needle) {
  return haystack.find(needle) != std::string::npos;
}

} // namespace

const TestSuite codegenSuite{
    "codegen",
    {
        {"globals and constant initializers",
         [](Tester &t) {
           const std::string out =
               ir("int g = 2 * 21; int a[3] = {1, 2}; char s[] = \"hi\"; "
                  "char *p = \"yo\"; int *q = &g; float f = 1;");
           t.expect(contains(out, "@g = global i32 42"), out);
           t.expect(
               contains(out, "@a = global [3 x i32] [i32 1, i32 2, i32 0]"),
               out);
           t.expect(contains(out, "@s = global [3 x i8] c\"hi\\00\""), out);
           t.expect(contains(out, "@p = global ptr @.str"), out);
           t.expect(contains(out, "@q = global ptr @g"), out);
           t.expect(contains(out, "@f = global float 1.0"), out);
         }},
        {"every construct produces verified IR",
         [](Tester &t) {
           const std::string out = ir(R"(
             int printf(char *fmt, ...);
             int sum(int *a, int n) {
               int s = 0, i = 0;
               while (i < n) {
                 if (a[i] < 0 || !a[i]) { i = i + 1; continue; }
                 if (s > 100 && n) break; else s = s + a[i];
                 i = i + 1;
               }
               return s;
             }
             int main() {
               int xs[4] = {1, -2, 3};
               char c = 'a';
               float f = c / 2.0;
               int *p = xs + 1;
               printf("%d %f %d\n", sum(xs, 4), f, (int)(p - xs));
               return 0;
             }
           )");
           t.expect(!contains(out, "error:"), out);
           t.expect(contains(out, "call i32 (ptr, ...) @printf"), out);
           t.expect(contains(out, "fpext float"),
                    out);                          // variadic float -> double
           t.expect(contains(out, "phi i1"), out); // short-circuit ||, &&
         }},
        {"missing return yields zero",
         [](Tester &t) {
           const std::string out = ir("int f() { }");
           t.expect(contains(out, "ret i32 0"), out);
         }},
    }};

} // namespace jia::test
