//===- Sema.cpp - Semantic analysis ---------------------------------------===//
//
// One walk over the tree. Expressions are checked bottom-up: an Expr whose
// type is still nullptr after checking had an error, which has already
// been reported; its parents then fail silently to avoid cascades.
// See docs/design/sema.md.
//
//===----------------------------------------------------------------------===//
#include "jia/Sema/Sema.h"

#include "jia/AST/ConstEval.h"
#include "jia/Basic/Overloaded.h"

#include <format>
#include <map>
#include <set>

namespace jia {
namespace {

struct Symbol {
  SymbolId id;
  const Type *type;
  bool isConst;
  bool isFunction;
  bool isDefined; // functions only: has a body
};

using Scope = std::map<std::string, Symbol, std::less<>>;

/// Picks the cast between two arithmetic types.
CastKind arithmeticCast(const Type *from, const Type *to) {
  if (from == to)
    return CastKind::NoOp;
  if (from->is(TypeKind::Float))
    return CastKind::FloatToInt;
  if (to->is(TypeKind::Float))
    return CastKind::IntToFloat;
  return CastKind::IntegralCast;
}

/// Wraps \p e in an implicit cast to \p to.
void wrap(Expr &e, CastKind kind, const Type *to) {
  const SourceLocation loc = e.loc;
  Box<Expr> inner = box(std::move(e));
  e = Expr{.node = Cast{kind, true, std::move(inner)}, .loc = loc, .type = to};
}

bool isNullConstant(const Expr &e) {
  if (!e.type || !e.type->isInteger())
    return false;
  const auto value = evaluateInteger(e);
  return value && *value == 0;
}

class Analyzer {
public:
  explicit Analyzer(TypeContext &types) : types_(types) { scopes_.emplace_back(); }

  Diagnostics run(std::vector<TopLevelDecl> &decls) {
    for (TopLevelDecl &decl : decls)
      std::visit(Overloaded{
                     [&](VarDecl &var) { declareVar(var, /*global=*/true); },
                     [&](FunctionDecl &fn) { declareFunction(fn); },
                 },
                 decl);
    return std::move(diags_);
  }

private:
  //===--- Helpers ---------------------------------------------------===//

  void error(SourceLocation loc, std::string message) {
    diags_.push_back(makeError(loc, std::move(message)));
  }

  /// Marks \p e as erroneous after reporting \p message.
  void fail(Expr &e, std::string message) {
    error(e.loc, std::move(message));
    e.type = nullptr;
  }

  const Symbol *lookup(std::string_view name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope)
      if (const auto it = scope->find(name); it != scope->end())
        return &it->second;
    return nullptr;
  }

  SymbolId newId() { return static_cast<SymbolId>(nextId_++); }

  //===--- Declarations ----------------------------------------------===//

  void declareFunction(FunctionDecl &fn) {
    Scope &globals = scopes_.front();
    if (const auto it = globals.find(fn.name); it != globals.end()) {
      Symbol &prior = it->second;
      if (!prior.isFunction || prior.type != fn.type) {
        error(fn.loc, std::format("conflicting types for '{}'", fn.name));
        return;
      }
      if (prior.isDefined && fn.body) {
        error(fn.loc, std::format("redefinition of '{}'", fn.name));
        return;
      }
      prior.isDefined |= fn.body.has_value();
      fn.symbol = prior.id;
    } else {
      fn.symbol = newId();
      globals.emplace(fn.name, Symbol{fn.symbol, fn.type, false, true,
                                      fn.body.has_value()});
    }
    if (!fn.body)
      return;

    // Parameters live in the same scope as the body's top-level names.
    scopes_.emplace_back();
    for (ParamDecl &param : fn.params) {
      if (param.name.empty()) {
        error(param.loc, "parameter name omitted in function definition");
        continue;
      }
      if (scopes_.back().contains(param.name)) {
        error(param.loc, std::format("redefinition of parameter '{}'", param.name));
        continue;
      }
      param.symbol = newId();
      scopes_.back().emplace(param.name,
                             Symbol{param.symbol, param.type, param.isConst, false, false});
    }
    returnType_ = fn.type->returnType();
    for (Stmt &stmt : fn.body->body)
      check(stmt);
    scopes_.pop_back();
  }

  void declareVar(VarDecl &var, bool global) {
    if (var.type->isArray() && var.type->length() == 0)
      completeArrayType(var);
    if (!var.type->size()) {
      error(var.loc, std::format("variable '{}' has incomplete type '{}'",
                                 var.name, var.type->str()));
      return;
    }
    if (scopes_.back().contains(var.name)) {
      error(var.loc, std::format("redefinition of '{}'", var.name));
      return;
    }
    var.symbol = newId();
    scopes_.back().emplace(var.name,
                           Symbol{var.symbol, var.type, var.isConst, false, false});
    if (global)
      globals_.insert(var.symbol);
    if (var.init)
      checkInit(*var.init, var.type, global);
  }

  /// "int a[] = {1, 2}" has type int[2]; "char s[] = \"hi\"" has char[3].
  void completeArrayType(VarDecl &var) {
    if (!var.init)
      return; // reported as incomplete by the caller
    std::uint64_t length = 0;
    if (const auto *list = std::get_if<std::vector<Initializer>>(&var.init->value))
      length = list->size();
    else if (const auto *expr = std::get_if<Expr>(&var.init->value))
      if (const auto *str = expr->as<StringLiteral>())
        length = str->bytes.size() + 1;
    if (length > 0)
      var.type = types_.arrayOf(var.type->element(), length);
  }

  void checkInit(Initializer &init, const Type *target, bool global) {
    auto *list = std::get_if<std::vector<Initializer>>(&init.value);
    if (target->isArray()) {
      Expr *expr = std::get_if<Expr>(&init.value);
      if (expr && expr->as<StringLiteral>() &&
          target->element()->is(TypeKind::Char)) {
        const std::size_t size = expr->as<StringLiteral>()->bytes.size();
        // C allows the terminating NUL to be dropped when it does not fit.
        if (size > target->length())
          error(init.loc, "initializer-string for char array is too long");
        expr->type = types_.arrayOf(types_.charType(), size + 1);
        expr->category = ValueCategory::LValue;
        return;
      }
      if (!list) {
        error(init.loc, "array initializer must be an initializer list");
        return;
      }
      if (list->size() > target->length())
        error((*list)[target->length()].loc, "excess elements in array initializer");
      for (Initializer &item : *list)
        checkInit(item, target->element(), global);
      return;
    }

    if (list) {
      if (list->size() != 1) {
        error(init.loc, list->empty() ? "scalar initializer cannot be empty"
                                      : "excess elements in scalar initializer");
        return;
      }
      checkInit(list->front(), target, global);
      return;
    }

    Expr &expr = *std::get_if<Expr>(&init.value); // not a list, so an Expr
    checkValue(expr);
    if (!expr.type || !convert(expr, target, "initializing"))
      return;
    if (global && !isConstantInit(expr))
      error(expr.loc, "initializer element is not a compile-time constant");
  }

  /// Global initializers must be constants, or addresses of globals and
  /// string literals.
  bool isConstantInit(const Expr &e) const {
    if (evaluateConstant(e))
      return true;
    if (const auto *cast = e.as<Cast>()) {
      const Expr &inner = *cast->operand;
      switch (cast->kind) {
      case CastKind::NullToPointer:
        return true;
      case CastKind::PointerCast:
        return isConstantInit(inner);
      case CastKind::ArrayToPointer:
        return inner.as<StringLiteral>() || isGlobalRef(inner);
      default:
        return false;
      }
    }
    if (const auto *unary = e.as<Unary>())
      return unary->op == UnaryOp::AddrOf && isGlobalRef(*unary->operand);
    return false;
  }

  bool isGlobalRef(const Expr &e) const {
    const auto *ref = e.as<DeclRef>();
    return ref && globals_.contains(ref->symbol);
  }

  //===--- Statements ------------------------------------------------===//

  void check(Stmt &stmt) {
    std::visit(
        Overloaded{
            [](NullStmt &) {},
            [&](ExprStmt &s) { checkValue(s.expr); },
            [&](DeclStmt &s) {
              for (VarDecl &var : s.vars)
                declareVar(var, /*global=*/false);
            },
            [&](CompoundStmt &s) {
              scopes_.emplace_back();
              for (Stmt &inner : s.body)
                check(inner);
              scopes_.pop_back();
            },
            [&](IfStmt &s) {
              checkCondition(s.cond);
              check(*s.then);
              if (s.otherwise)
                check(*s.otherwise);
            },
            [&](WhileStmt &s) {
              checkCondition(s.cond);
              ++loopDepth_;
              check(*s.body);
              --loopDepth_;
            },
            [&](BreakStmt &) {
              if (loopDepth_ == 0)
                error(stmt.loc, "'break' statement not in loop statement");
            },
            [&](ContinueStmt &) {
              if (loopDepth_ == 0)
                error(stmt.loc, "'continue' statement not in loop statement");
            },
            [&](ReturnStmt &s) { checkReturn(s, stmt.loc); },
        },
        stmt.node);
  }

  void checkCondition(Expr &cond) {
    checkValue(cond);
    if (cond.type && !cond.type->isScalar())
      fail(cond, std::format("statement requires expression of scalar type "
                             "('{}' invalid)", cond.type->str()));
  }

  void checkReturn(ReturnStmt &ret, SourceLocation loc) {
    if (!ret.value) {
      if (!returnType_->isVoid())
        error(loc, "non-void function should return a value");
      return;
    }
    Expr &value = *ret.value;
    checkValue(value);
    if (!value.type)
      return;
    if (returnType_->isVoid()) {
      if (!value.type->isVoid())
        error(value.loc, "void function should not return a value");
      return;
    }
    convert(value, returnType_, "returning");
  }

  //===--- Conversions -----------------------------------------------===//

  /// Checks \p e and applies the conversions every value undergoes: arrays
  /// decay to pointers, and functions cannot be used as values.
  void checkValue(Expr &e) {
    check(e);
    if (!e.type)
      return;
    if (e.type->isArray())
      wrap(e, CastKind::ArrayToPointer, types_.pointerTo(e.type->element()));
    else if (e.type->isFunction())
      fail(e, "functions cannot be used as values (function pointers are not "
              "supported yet)");
  }

  void toArithmetic(Expr &e, const Type *to) {
    if (e.type != to)
      wrap(e, arithmeticCast(e.type, to), to);
  }

  /// Integer promotion: char becomes int in arithmetic.
  void promote(Expr &e) {
    if (e.type->is(TypeKind::Char))
      toArithmetic(e, types_.intType());
  }

  /// The usual arithmetic conversions: both operands get a common type.
  const Type *usualArithmetic(Expr &a, Expr &b) {
    const Type *common =
        a.type->is(TypeKind::Float) || b.type->is(TypeKind::Float)
            ? types_.floatType()
            : types_.intType();
    toArithmetic(a, common);
    toArithmetic(b, common);
    return common;
  }

  /// Converts \p e to \p to as if by assignment. \p context describes the
  /// situation for the error message ("initializing", "returning", ...).
  bool convert(Expr &e, const Type *to, std::string_view context) {
    const Type *from = e.type;
    if (from == to)
      return true;
    if (from->isArithmetic() && to->isArithmetic()) {
      toArithmetic(e, to);
      return true;
    }
    if (to->isPointer()) {
      if (from->isPointer() &&
          (from->pointee()->isVoid() || to->pointee()->isVoid())) {
        wrap(e, CastKind::PointerCast, to);
        return true;
      }
      if (isNullConstant(e)) {
        wrap(e, CastKind::NullToPointer, to);
        return true;
      }
    }
    fail(e, std::format("incompatible types when {} '{}' from '{}'", context,
                        to->str(), from->str()));
    return false;
  }

  //===--- Expressions -----------------------------------------------===//

  void check(Expr &e) {
    // Desugarings replace the node itself, so do them before visiting.
    if (auto *index = e.as<Index>()) {
      Box<Expr> base = std::move(index->base);
      Box<Expr> offset = std::move(index->index);
      const SourceLocation loc = e.loc;
      Expr sum{.node = Binary{BinaryOp::Add, std::move(base), std::move(offset)},
               .loc = loc};
      e = Expr{.node = Unary{UnaryOp::Deref, box(std::move(sum))}, .loc = loc};
    }
    if (e.as<SizeOfType>() || e.as<SizeOfExpr>()) {
      checkSizeOf(e);
      return;
    }

    std::visit(
        Overloaded{
            [&](IntLiteral &) { e.type = types_.intType(); },
            [&](FloatLiteral &) { e.type = types_.floatType(); },
            [&](StringLiteral &s) {
              e.type = types_.arrayOf(types_.charType(), s.bytes.size() + 1);
              e.category = ValueCategory::LValue;
            },
            [&](DeclRef &ref) { checkDeclRef(e, ref); },
            [&](Unary &u) { checkUnary(e, u); },
            [&](Binary &b) { checkBinary(e, b); },
            [&](Assign &a) { checkAssign(e, a); },
            [&](Call &c) { checkCall(e, c); },
            [&](Cast &c) { checkCast(e, c); },
            [](Index &) {},
            [](SizeOfType &) {},
            [](SizeOfExpr &) {},
        },
        e.node);
  }

  void checkSizeOf(Expr &e) {
    const Type *type = nullptr;
    if (const auto *s = e.as<SizeOfType>()) {
      type = s->operand;
    } else {
      Expr &operand = *e.as<SizeOfExpr>()->operand;
      check(operand); // no decay: sizeof an array is the whole array
      if (!operand.type) {
        e.type = nullptr;
        return;
      }
      type = operand.type;
    }
    const auto size = type->size();
    if (!size) {
      fail(e, std::format("invalid application of 'sizeof' to type '{}'", type->str()));
      return;
    }
    const SourceLocation loc = e.loc;
    e = Expr{.node = IntLiteral{static_cast<std::int64_t>(*size)},
             .loc = loc,
             .type = types_.intType()};
  }

  void checkDeclRef(Expr &e, DeclRef &ref) {
    const Symbol *symbol = lookup(ref.name);
    if (!symbol) {
      fail(e, std::format("use of undeclared identifier '{}'", ref.name));
      return;
    }
    ref.symbol = symbol->id;
    e.type = symbol->type;
    e.category = symbol->isFunction ? ValueCategory::RValue : ValueCategory::LValue;
  }

  void checkUnary(Expr &e, Unary &u) {
    Expr &operand = *u.operand;
    if (u.op == UnaryOp::AddrOf) {
      check(operand);
      if (!operand.type)
        return void(e.type = nullptr);
      if (!operand.isLValue())
        return fail(e, std::format("cannot take the address of an rvalue of type '{}'",
                                   operand.type->str()));
      e.type = types_.pointerTo(operand.type);
      return;
    }

    checkValue(operand);
    if (!operand.type)
      return void(e.type = nullptr);
    switch (u.op) {
    case UnaryOp::Plus:
    case UnaryOp::Minus:
      if (!operand.type->isArithmetic())
        return fail(e, std::format("invalid argument type '{}' to unary expression",
                                   operand.type->str()));
      promote(operand);
      e.type = operand.type;
      return;
    case UnaryOp::Not:
      if (!operand.type->isScalar())
        return fail(e, std::format("invalid argument type '{}' to unary expression",
                                   operand.type->str()));
      e.type = types_.intType();
      return;
    case UnaryOp::Deref:
      if (!operand.type->isPointer())
        return fail(e, std::format("indirection requires pointer operand ('{}' invalid)",
                                   operand.type->str()));
      if (operand.type->pointee()->isVoid())
        return fail(e, "cannot dereference a 'void *' pointer");
      e.type = operand.type->pointee();
      e.category = ValueCategory::LValue;
      return;
    case UnaryOp::AddrOf:
      return;
    }
  }

  void checkBinary(Expr &e, Binary &b) {
    checkValue(*b.lhs);
    checkValue(*b.rhs);
    if (!b.lhs->type || !b.rhs->type)
      return void(e.type = nullptr);
    Expr &l = *b.lhs, &r = *b.rhs;
    const Type *lt = l.type, *rt = r.type;
    const auto invalid = [&] {
      fail(e, std::format("invalid operands to binary expression ('{}' and '{}')",
                          lt->str(), rt->str()));
    };
    const auto pointerArithmeticOk = [&](const Type *ptr) {
      if (ptr->pointee()->size())
        return true;
      fail(e, std::format("arithmetic on a pointer to incomplete type '{}'",
                          ptr->pointee()->str()));
      return false;
    };

    switch (b.op) {
    case BinaryOp::LogAnd:
    case BinaryOp::LogOr:
      if (!lt->isScalar() || !rt->isScalar())
        return invalid();
      e.type = types_.intType();
      return;
    case BinaryOp::Mul:
    case BinaryOp::Div:
      if (!lt->isArithmetic() || !rt->isArithmetic())
        return invalid();
      e.type = usualArithmetic(l, r);
      return;
    case BinaryOp::Rem:
      if (!lt->isInteger() || !rt->isInteger())
        return invalid();
      e.type = usualArithmetic(l, r);
      return;
    case BinaryOp::Add:
      if (lt->isArithmetic() && rt->isArithmetic()) {
        e.type = usualArithmetic(l, r);
        return;
      }
      if (lt->isInteger() && rt->isPointer())
        std::swap(b.lhs, b.rhs); // keep the pointer on the left
      if (b.lhs->type->isPointer() && b.rhs->type->isInteger()) {
        if (!pointerArithmeticOk(b.lhs->type))
          return;
        promote(*b.rhs);
        e.type = b.lhs->type;
        return;
      }
      return invalid();
    case BinaryOp::Sub:
      if (lt->isArithmetic() && rt->isArithmetic()) {
        e.type = usualArithmetic(l, r);
        return;
      }
      if (lt->isPointer() && rt->isInteger()) {
        if (!pointerArithmeticOk(lt))
          return;
        promote(r);
        e.type = lt;
        return;
      }
      if (lt->isPointer() && lt == rt) {
        if (!pointerArithmeticOk(lt))
          return;
        e.type = types_.intType();
        return;
      }
      return invalid();
    case BinaryOp::Lt:
    case BinaryOp::Gt:
    case BinaryOp::Le:
    case BinaryOp::Ge:
    case BinaryOp::Eq:
    case BinaryOp::Ne:
      e.type = types_.intType();
      if (lt->isArithmetic() && rt->isArithmetic()) {
        usualArithmetic(l, r);
        return;
      }
      if (lt->isPointer() && rt->isPointer()) {
        if (lt != rt && !lt->pointee()->isVoid() && !rt->pointee()->isVoid())
          return fail(e, std::format("comparison of distinct pointer types ('{}' and '{}')",
                                     lt->str(), rt->str()));
        if (lt != rt)
          wrap(r, CastKind::PointerCast, lt);
        return;
      }
      if (lt->isPointer() && isNullConstant(r))
        return wrap(r, CastKind::NullToPointer, lt);
      if (rt->isPointer() && isNullConstant(l))
        return wrap(l, CastKind::NullToPointer, rt);
      return invalid();
    }
  }

  void checkAssign(Expr &e, Assign &a) {
    check(*a.target);
    checkValue(*a.value);
    Expr &target = *a.target;
    if (!target.type || !a.value->type)
      return void(e.type = nullptr);
    if (!target.isLValue())
      return fail(e, "expression is not assignable");
    if (target.type->isArray())
      return fail(e, std::format("array type '{}' is not assignable", target.type->str()));
    if (const auto *ref = target.as<DeclRef>()) {
      const Symbol *symbol = lookup(ref->name);
      if (symbol && symbol->isConst)
        return fail(e, std::format("cannot assign to variable '{}' with "
                                   "const-qualified type", ref->name));
    }
    if (!convert(*a.value, target.type, "assigning to"))
      return void(e.type = nullptr);
    e.type = target.type;
  }

  void checkCall(Expr &e, Call &call) {
    Expr &callee = *call.callee;
    check(callee);
    for (Expr &arg : call.args)
      checkValue(arg);
    if (!callee.type)
      return void(e.type = nullptr);
    if (!callee.type->isFunction())
      return fail(e, std::format("called object type '{}' is not a function",
                                 callee.type->str()));

    const Type *fn = callee.type;
    const std::size_t expected = fn->params().size(), given = call.args.size();
    if (given < expected || (given > expected && !fn->isVariadic()))
      return fail(e, std::format("too {} arguments to function call, expected {}, have {}",
                                 given < expected ? "few" : "many", expected, given));
    bool ok = true;
    for (std::size_t i = 0; i < given; ++i) {
      Expr &arg = call.args[i];
      if (!arg.type) {
        ok = false;
        continue;
      }
      if (i < expected)
        ok &= convert(arg, fn->params()[i], "passing");
      else if (arg.type->isVoid())
        ok = false, fail(arg, "argument may not have 'void' type");
      else
        promote(arg); // default argument promotions; floats widen in codegen
    }
    e.type = ok ? fn->returnType() : nullptr;
  }

  void checkCast(Expr &e, Cast &cast) {
    if (cast.kind != CastKind::Unresolved)
      return; // implicit casts are created already checked
    checkValue(*cast.operand);
    const Type *from = cast.operand->type, *to = e.type;
    if (!from)
      return void(e.type = nullptr);
    if (to->isVoid())
      cast.kind = CastKind::ToVoid;
    else if (from->isArithmetic() && to->isArithmetic())
      cast.kind = arithmeticCast(from, to);
    else if (from->isPointer() && to->isPointer())
      cast.kind = CastKind::PointerCast;
    else if (from->isInteger() && to->isPointer())
      cast.kind = CastKind::IntToPointer;
    else if (from->isPointer() && to->isInteger())
      cast.kind = CastKind::PointerToInt;
    else
      fail(e, std::format("cannot cast from '{}' to '{}'", from->str(), to->str()));
  }

  TypeContext &types_;
  std::vector<Scope> scopes_;
  std::set<SymbolId> globals_;
  Diagnostics diags_;
  std::uint32_t nextId_ = 1;
  const Type *returnType_ = nullptr;
  int loopDepth_ = 0;
};

} // namespace

Result<TranslationUnit> analyze(TranslationUnit tu) {
  Diagnostics diags = Analyzer(tu.types).run(tu.decls);
  if (!diags.empty())
    return std::unexpected(std::move(diags));
  return tu;
}

} // namespace jia
