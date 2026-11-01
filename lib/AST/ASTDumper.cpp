//===- ASTDumper.cpp - Print a tree as text -------------------------------===//
#include "jia/AST/ASTDumper.h"

#include "jia/Basic/Overloaded.h"

#include <format>

namespace jia {

std::string_view name(UnaryOp op) {
  switch (op) {
  case UnaryOp::Plus: return "+";
  case UnaryOp::Minus: return "-";
  case UnaryOp::Not: return "!";
  case UnaryOp::Deref: return "*";
  case UnaryOp::AddrOf: return "&";
  }
  return "?";
}

std::string_view name(BinaryOp op) {
  switch (op) {
  case BinaryOp::Mul: return "*";
  case BinaryOp::Div: return "/";
  case BinaryOp::Rem: return "%";
  case BinaryOp::Add: return "+";
  case BinaryOp::Sub: return "-";
  case BinaryOp::Lt: return "<";
  case BinaryOp::Gt: return ">";
  case BinaryOp::Le: return "<=";
  case BinaryOp::Ge: return ">=";
  case BinaryOp::Eq: return "==";
  case BinaryOp::Ne: return "!=";
  case BinaryOp::LogAnd: return "&&";
  case BinaryOp::LogOr: return "||";
  }
  return "?";
}

std::string_view name(CastKind kind) {
  switch (kind) {
  case CastKind::Unresolved: return "Unresolved";
  case CastKind::NoOp: return "NoOp";
  case CastKind::IntegralCast: return "IntegralCast";
  case CastKind::IntToFloat: return "IntToFloat";
  case CastKind::FloatToInt: return "FloatToInt";
  case CastKind::ArrayToPointer: return "ArrayToPointer";
  case CastKind::NullToPointer: return "NullToPointer";
  case CastKind::PointerCast: return "PointerCast";
  case CastKind::PointerToInt: return "PointerToInt";
  case CastKind::IntToPointer: return "IntToPointer";
  case CastKind::ToVoid: return "ToVoid";
  }
  return "?";
}

namespace {

/// Escapes a decoded string so it prints on one line.
std::string quote(std::string_view bytes) {
  std::string out = "\"";
  for (const char c : bytes) {
    switch (c) {
    case '\n': out += "\\n"; break;
    case '\t': out += "\\t"; break;
    case '\0': out += "\\0"; break;
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    default: out += c;
    }
  }
  return out + "\"";
}

class Dumper {
public:
  std::string run(const TranslationUnit &tu) {
    line(0, "TranslationUnit");
    for (const TopLevelDecl &decl : tu.decls)
      std::visit([&](const auto &d) { dump(d, 1); }, decl);
    return std::move(out_);
  }

private:
  void line(int depth, const std::string &text) {
    out_ += std::string(static_cast<std::size_t>(depth) * 2, ' ');
    out_ += text;
    out_ += '\n';
  }

  static std::string typeSuffix(const Type *type) {
    return type ? std::format(" '{}'", type->str()) : "";
  }

  void dump(const VarDecl &var, int depth) {
    line(depth, std::format("VarDecl {}{}{}", var.name, typeSuffix(var.type),
                            var.isConst ? " const" : ""));
    if (var.init)
      dump(*var.init, depth + 1);
  }

  void dump(const Initializer &init, int depth) {
    std::visit(Overloaded{
                   [&](const Expr &e) { dump(e, depth); },
                   [&](const std::vector<Initializer> &list) {
                     line(depth, "InitList");
                     for (const Initializer &item : list)
                       dump(item, depth + 1);
                   },
               },
               init.value);
  }

  void dump(const FunctionDecl &fn, int depth) {
    line(depth, std::format("FunctionDecl {}{}{}", fn.name, typeSuffix(fn.type),
                            fn.body ? "" : " prototype"));
    for (const ParamDecl &param : fn.params)
      line(depth + 1, std::format("ParamDecl {}{}", param.name,
                                  typeSuffix(param.type)));
    if (fn.body)
      dump(*fn.body, depth + 1);
  }

  void dump(const CompoundStmt &block, int depth) {
    line(depth, "CompoundStmt");
    for (const Stmt &stmt : block.body)
      dump(stmt, depth + 1);
  }

  void dump(const Stmt &stmt, int depth) {
    std::visit(
        Overloaded{
            [&](const NullStmt &) { line(depth, "NullStmt"); },
            [&](const ExprStmt &s) {
              line(depth, "ExprStmt");
              dump(s.expr, depth + 1);
            },
            [&](const DeclStmt &s) {
              line(depth, "DeclStmt");
              for (const VarDecl &var : s.vars)
                dump(var, depth + 1);
            },
            [&](const CompoundStmt &s) { dump(s, depth); },
            [&](const IfStmt &s) {
              line(depth, "IfStmt");
              dump(s.cond, depth + 1);
              dump(*s.then, depth + 1);
              if (s.otherwise)
                dump(*s.otherwise, depth + 1);
            },
            [&](const WhileStmt &s) {
              line(depth, "WhileStmt");
              dump(s.cond, depth + 1);
              dump(*s.body, depth + 1);
            },
            [&](const BreakStmt &) { line(depth, "BreakStmt"); },
            [&](const ContinueStmt &) { line(depth, "ContinueStmt"); },
            [&](const ReturnStmt &s) {
              line(depth, "ReturnStmt");
              if (s.value)
                dump(*s.value, depth + 1);
            },
        },
        stmt.node);
  }

  void dump(const Expr &expr, int depth) {
    const std::string suffix = typeSuffix(expr.type) +
                               (expr.isLValue() ? " lvalue" : "");
    std::visit(
        Overloaded{
            [&](const IntLiteral &e) {
              line(depth, std::format("IntLiteral {}{}", e.value, suffix));
            },
            [&](const FloatLiteral &e) {
              line(depth, std::format("FloatLiteral {}{}", e.value, suffix));
            },
            [&](const StringLiteral &e) {
              line(depth, std::format("StringLiteral {}{}", quote(e.bytes), suffix));
            },
            [&](const DeclRef &e) {
              line(depth, std::format("DeclRef {}{}", e.name, suffix));
            },
            [&](const Unary &e) {
              line(depth, std::format("Unary {}{}", name(e.op), suffix));
              dump(*e.operand, depth + 1);
            },
            [&](const Binary &e) {
              line(depth, std::format("Binary {}{}", name(e.op), suffix));
              dump(*e.lhs, depth + 1);
              dump(*e.rhs, depth + 1);
            },
            [&](const Assign &e) {
              line(depth, "Assign" + suffix);
              dump(*e.target, depth + 1);
              dump(*e.value, depth + 1);
            },
            [&](const Call &e) {
              line(depth, "Call" + suffix);
              dump(*e.callee, depth + 1);
              for (const Expr &arg : e.args)
                dump(arg, depth + 1);
            },
            [&](const Index &e) {
              line(depth, "Index" + suffix);
              dump(*e.base, depth + 1);
              dump(*e.index, depth + 1);
            },
            [&](const Cast &e) {
              line(depth, std::format("Cast {}{}{}", name(e.kind),
                                      e.isImplicit ? " implicit" : "", suffix));
              dump(*e.operand, depth + 1);
            },
            [&](const SizeOfType &e) {
              line(depth, std::format("SizeOfType '{}'{}", e.operand->str(), suffix));
            },
            [&](const SizeOfExpr &e) {
              line(depth, "SizeOfExpr" + suffix);
              dump(*e.operand, depth + 1);
            },
        },
        expr.node);
  }

  std::string out_;
};

} // namespace

std::string dumpAST(const TranslationUnit &tu) { return Dumper().run(tu); }

} // namespace jia
