#include "Suites.h"
#include "jia/AST/ConstEval.h"

namespace jia::test {
namespace {

Expr lit(std::int64_t v) { return Expr{.node = IntLiteral{v}}; }

Expr bin(BinaryOp op, Expr l, Expr r) {
  return Expr{.node = Binary{op, std::make_unique<Expr>(std::move(l)),
                             std::make_unique<Expr>(std::move(r))}};
}

} // namespace

const TestSuite astSuite{
    "ast",
    {
        {"types are uniqued",
         [](Tester &t) {
           TypeContext ctx;
           t.expect(ctx.pointerTo(ctx.intType()) ==
                    ctx.pointerTo(ctx.intType()));
           t.expect(ctx.arrayOf(ctx.charType(), 3) !=
                    ctx.arrayOf(ctx.charType(), 4));
           t.expect(ctx.function(ctx.intType(), {ctx.intType()}, false) ==
                    ctx.function(ctx.intType(), {ctx.intType()}, false));
         }},
        {"types print like C",
         [](Tester &t) {
           TypeContext ctx;
           const Type *arr = ctx.arrayOf(ctx.arrayOf(ctx.intType(), 3), 2);
           t.expectEq(arr->str(), std::string("int[2][3]"));
           t.expectEq(ctx.pointerTo(ctx.pointerTo(ctx.charType()))->str(),
                      std::string("char **"));
           t.expectEq(ctx.pointerTo(ctx.arrayOf(ctx.intType(), 3))->str(),
                      std::string("int(*)[3]"));
           t.expectEq(ctx.function(ctx.intType(),
                                   {ctx.pointerTo(ctx.charType())}, true)
                          ->str(),
                      std::string("int(char *, ...)"));
         }},
        {"type sizes",
         [](Tester &t) {
           TypeContext ctx;
           t.expectEq(*ctx.arrayOf(ctx.intType(), 10)->size(),
                      std::uint64_t{40});
           t.expectEq(*ctx.pointerTo(ctx.voidType())->size(), std::uint64_t{8});
           t.expect(!ctx.voidType()->size().has_value());
         }},
        {"folds integer arithmetic",
         [](Tester &t) {
           const Expr e =
               bin(BinaryOp::Mul, bin(BinaryOp::Add, lit(2), lit(3)), lit(4));
           t.expectEq(*evaluateInteger(e), std::int64_t{20});
           t.expect(!evaluateInteger(bin(BinaryOp::Div, lit(1), lit(0))));
           t.expectEq(*evaluateInteger(bin(BinaryOp::LogOr, lit(0), lit(7))),
                      std::int64_t{1});
         }},
        {"narrows to the expression type",
         [](Tester &t) {
           TypeContext ctx;
           Expr e = lit(300);
           e.type = ctx.charType();
           t.expectEq(*evaluateInteger(e), std::int64_t{44});
         }},
    }};

} // namespace jia::test
