#include "jia/Sema/Sema.h"
#include "Suites.h"
#include "jia/AST/ASTDumper.h"
#include "jia/Lex/Lexer.h"
#include "jia/Parse/Parser.h"

namespace jia::test {
namespace {

Result<TranslationUnit> frontEnd(std::string_view source) {
  return lex(source)
      .and_then([](std::vector<Token> tokens) { return parse(tokens); })
      .and_then(analyze);
}

/// The typed AST of the body of the last function, or "error: <first>".
std::string typed(std::string_view source) {
  auto tu = frontEnd(source);
  if (!tu)
    return "error: " + tu.error().front().message;
  const std::string dump = dumpAST(*tu);
  return dump.substr(dump.rfind("    CompoundStmt\n") + 17);
}

std::string firstError(std::string_view source) {
  auto tu = frontEnd(source);
  return tu ? "ok" : tu.error().front().message;
}

} // namespace

const TestSuite semaSuite{
    "sema",
    {
        {"usual arithmetic conversions",
         [](Tester &t) {
           t.expectEq(typed("float f(char c) { return c + 1.5; }"),
                      std::string("      ReturnStmt\n"
                                  "        Binary + 'float'\n"
                                  "          Cast IntToFloat implicit 'float'\n"
                                  "            DeclRef c 'char' lvalue\n"
                                  "          FloatLiteral 1.5 'float'\n"));
         }},
        {"indexing becomes pointer arithmetic",
         [](Tester &t) {
           t.expectEq(typed("int f(int a[], int i) { return i[a]; }"),
                      std::string("      ReturnStmt\n"
                                  "        Unary * 'int' lvalue\n"
                                  "          Binary + 'int *'\n"
                                  "            DeclRef a 'int *' lvalue\n"
                                  "            DeclRef i 'int' lvalue\n"));
         }},
        {"arrays decay and sizeof folds",
         [](Tester &t) {
           t.expectEq(
               typed("int g(char *p); int f() { char s[8]; return g(s) + "
                     "sizeof s; }"),
               std::string("      DeclStmt\n"
                           "        VarDecl s 'char[8]'\n"
                           "      ReturnStmt\n"
                           "        Binary + 'int'\n"
                           "          Call 'int'\n"
                           "            DeclRef g 'int(char *)'\n"
                           "            Cast ArrayToPointer implicit 'char *'\n"
                           "              DeclRef s 'char[8]' lvalue\n"
                           "          IntLiteral 8 'int'\n"));
         }},
        {"null pointer constants and incomplete arrays",
         [](Tester &t) {
           t.expectEq(typed("void f() { int *p = 0; char s[] = \"hi\"; }"),
                      std::string(
                          "      DeclStmt\n"
                          "        VarDecl p 'int *'\n"
                          "          Cast NullToPointer implicit 'int *'\n"
                          "            IntLiteral 0 'int'\n"
                          "      DeclStmt\n"
                          "        VarDecl s 'char[3]'\n"
                          "          StringLiteral \"hi\" 'char[3]' lvalue\n"));
         }},
        {"names and scopes",
         [](Tester &t) {
           t.expectEq(firstError("int f() { return x; }"),
                      std::string("use of undeclared identifier 'x'"));
           t.expectEq(firstError("int f() { int a; { int a; } int a; }"),
                      std::string("redefinition of 'a'"));
           t.expectEq(firstError("int f(int a) { int a; }"),
                      std::string("redefinition of 'a'"));
           t.expectEq(firstError("int f(); float f();"),
                      std::string("conflicting types for 'f'"));
           t.expectEq(
               firstError(
                   "int f(); int f() { return 1; } int f() { return 2; }"),
               std::string("redefinition of 'f'"));
         }},
        {"type errors",
         [](Tester &t) {
           t.expectEq(firstError("void f() { int *p; float x; p = x; }"),
                      std::string("incompatible types when assigning to 'int "
                                  "*' from 'float'"));
           t.expectEq(firstError("void f() { 1 = 2; }"),
                      std::string("expression is not assignable"));
           t.expectEq(
               firstError("void f() { const int c = 1; c = 2; }"),
               std::string(
                   "cannot assign to variable 'c' with const-qualified type"));
           // In "const int *p" the pointee is const, not p.
           t.expectEq(firstError("void f(const int *p, int *q) { p = q; }"),
                      std::string("ok"));
           t.expectEq(firstError("void f() { float x; x % 2; }"),
                      std::string("invalid operands to binary expression "
                                  "('float' and 'int')"));
           t.expectEq(
               firstError("void f() { int x; *x; }"),
               std::string(
                   "indirection requires pointer operand ('int' invalid)"));
           t.expectEq(
               firstError("int g(int a); void f() { g(); }"),
               std::string(
                   "too few arguments to function call, expected 1, have 0"));
           t.expectEq(firstError("int f() { return; }"),
                      std::string("non-void function should return a value"));
           t.expectEq(firstError("void f() { break; }"),
                      std::string("'break' statement not in loop statement"));
         }},
        {"global initializers must be constant",
         [](Tester &t) {
           t.expectEq(firstError("int a = 2 * 3; int *p = &a; char *s = \"x\"; "
                                 "int b[2] = {1, 2};"),
                      std::string("ok"));
           t.expectEq(
               firstError("int a; int b = a;"),
               std::string(
                   "initializer element is not a compile-time constant"));
           t.expectEq(firstError("int b[1] = {1, 2};"),
                      std::string("excess elements in array initializer"));
         }},
        {"errors do not cascade",
         [](Tester &t) {
           auto tu = frontEnd("int f() { return (x + 1) * 2 - y; }");
           t.expectEq(tu.error().size(), std::size_t{2}); // x and y only
         }},
    }};

} // namespace jia::test
