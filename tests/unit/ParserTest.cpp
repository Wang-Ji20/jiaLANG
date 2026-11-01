#include "jia/Parse/Parser.h"
#include "Suites.h"
#include "jia/AST/ASTDumper.h"
#include "jia/Lex/Lexer.h"

namespace jia::test {
namespace {

/// The AST dump without the "TranslationUnit" line, or "error: <first>".
std::string parsed(std::string_view source) {
  auto tu = lex(source).and_then(
      [](std::vector<Token> tokens) { return parse(tokens); });
  if (!tu)
    return "error: " + tu.error().front().message;
  const std::string dump = dumpAST(*tu);
  return dump.substr(dump.find('\n') + 1);
}

std::size_t errorCount(std::string_view source) {
  auto tu = lex(source).and_then(
      [](std::vector<Token> tokens) { return parse(tokens); });
  return tu ? 0 : tu.error().size();
}

} // namespace

const TestSuite parserSuite{
    "parser",
    {
        {"precedence and associativity",
         [](Tester &t) {
           t.expectEq(parsed("int x = 1 + 2 * 3 - 4;"),
                      std::string("  VarDecl x 'int'\n"
                                  "    Binary -\n"
                                  "      Binary +\n"
                                  "        IntLiteral 1\n"
                                  "        Binary *\n"
                                  "          IntLiteral 2\n"
                                  "          IntLiteral 3\n"
                                  "      IntLiteral 4\n"));
         }},
        {"assignment is right-associative",
         [](Tester &t) {
           t.expectEq(parsed("void f() { a = b = 1; }"),
                      std::string("  FunctionDecl f 'void()'\n"
                                  "    CompoundStmt\n"
                                  "      ExprStmt\n"
                                  "        Assign\n"
                                  "          DeclRef a\n"
                                  "          Assign\n"
                                  "            DeclRef b\n"
                                  "            IntLiteral 1\n"));
         }},
        {"declarators",
         [](Tester &t) {
           t.expectEq(parsed("int *a[3], (*b)[4], c[2][5];"),
                      std::string("  VarDecl a 'int *[3]'\n"
                                  "  VarDecl b 'int(*)[4]'\n"
                                  "  VarDecl c 'int[2][5]'\n"));
           t.expectEq(parsed("char *f(int n, char s[], ...);"),
                      std::string("  FunctionDecl f 'char *(int, char *, ...)' "
                                  "prototype\n"
                                  "    ParamDecl n 'int'\n"
                                  "    ParamDecl s 'char *'\n"));
           t.expectEq(parsed("int g(void);"),
                      std::string("  FunctionDecl g 'int()' prototype\n"));
         }},
        {"array sizes are folded",
         [](Tester &t) {
           t.expectEq(parsed("int a[2 * 8];"),
                      std::string("  VarDecl a 'int[16]'\n"));
           t.expectEq(parsed("int a[0];"),
                      std::string("error: array size must be positive"));
         }},
        {"statements",
         [](Tester &t) {
           t.expectEq(
               parsed("void f() { while (x) if (y) break; else continue; }"),
               std::string("  FunctionDecl f 'void()'\n"
                           "    CompoundStmt\n"
                           "      WhileStmt\n"
                           "        DeclRef x\n"
                           "        IfStmt\n"
                           "          DeclRef y\n"
                           "          BreakStmt\n"
                           "          ContinueStmt\n"));
         }},
        {"casts, sizeof, postfix, and literals",
         [](Tester &t) {
           t.expectEq(
               parsed(
                   "int x = (int)sizeof(char *) + f(a[1], 'A', \"h\" \"i\");"),
               std::string("  VarDecl x 'int'\n"
                           "    Binary +\n"
                           "      Cast Unresolved 'int'\n"
                           "        SizeOfType 'char *'\n"
                           "      Call\n"
                           "        DeclRef f\n"
                           "        Index\n"
                           "          DeclRef a\n"
                           "          IntLiteral 1\n"
                           "        IntLiteral 65\n"
                           "        StringLiteral \"hi\"\n"));
         }},
        {"initializer lists",
         [](Tester &t) {
           t.expectEq(parsed("int a[] = {1, {2}, };"),
                      std::string("  VarDecl a 'int[]'\n"
                                  "    InitList\n"
                                  "      IntLiteral 1\n"
                                  "      InitList\n"
                                  "        IntLiteral 2\n"));
         }},
        {"reports errors and recovers",
         [](Tester &t) {
           t.expectEq(parsed("int x = ;"),
                      std::string("error: expected expression before ';'"));
           t.expectEq(parsed("void f() { x = 1 }"),
                      std::string("error: expected ';' before '}'"));
           t.expectEq(
               parsed("int (*f)(int);"),
               std::string("error: function pointers are not supported yet"));
           // Three broken statements, three errors.
           t.expectEq(
               errorCount("void f() { 1 +; 2 +; } int g() { return +; }"),
               std::size_t{3});
         }},
    }};

} // namespace jia::test
