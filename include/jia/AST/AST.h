//===- AST.h - The abstract syntax tree -------------------------*- C++ -*-===//
//
// The tree is built from plain structs and std::variant sum types; it has no
// behaviour of its own. Children are owned through Box (std::unique_ptr), so
// the tree is move-only and each node has exactly one owner.
//
// The parser fills in the syntax. Sema then consumes the tree and returns it
// with every Expr's `type` and `category` set, names resolved to SymbolIds,
// and implicit conversions made explicit as Cast nodes.
//
//===----------------------------------------------------------------------===//
#pragma once

#include "jia/AST/Type.h"
#include "jia/Basic/SourceLocation.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace jia {

template <class T> using Box = std::unique_ptr<T>;

/// Moves \p value onto the heap: box(expr) is Rust's Box::new(expr).
template <class T> [[nodiscard]] Box<T> box(T value) {
  return std::make_unique<T>(std::move(value));
}

/// Identifies a declared entity (variable, parameter, function). Sema
/// assigns ids; code generation maps them to LLVM values.
enum class SymbolId : std::uint32_t { None = 0 };

//===----------------------------------------------------------------------===//
// Expressions
//===----------------------------------------------------------------------===//

struct Expr;

enum class UnaryOp : std::uint8_t { Plus, Minus, Not, Deref, AddrOf };

enum class BinaryOp : std::uint8_t {
  Mul,
  Div,
  Rem,
  Add,
  Sub,
  Lt,
  Gt,
  Le,
  Ge,
  Eq,
  Ne,
  LogAnd,
  LogOr,
};

/// How a Cast converts its operand. The parser creates explicit casts as
/// Unresolved; Sema picks the kind.
enum class CastKind : std::uint8_t {
  Unresolved,
  NoOp,         // same representation, e.g. int to int
  IntegralCast, // char <-> int
  IntToFloat,
  FloatToInt,
  ArrayToPointer, // array "decays" to a pointer to its first element
  NullToPointer,  // the constant 0 used as a pointer
  PointerCast,    // T* to U*
  PointerToInt,
  IntToPointer,
  ToVoid, // (void)expr
};

struct IntLiteral {
  std::int64_t value;
};
struct FloatLiteral {
  double value;
};
struct StringLiteral {
  std::string bytes;
}; // decoded, without the NUL
struct DeclRef {
  std::string name;
  SymbolId symbol = SymbolId::None; // set by Sema
};
struct Unary {
  UnaryOp op;
  Box<Expr> operand;
};
struct Binary {
  BinaryOp op;
  Box<Expr> lhs, rhs;
};
struct Assign {
  Box<Expr> target, value;
};
struct Call {
  Box<Expr> callee;
  std::vector<Expr> args;
};
/// a[i]. Sema rewrites it to *(a + i), so later phases never see it.
struct Index {
  Box<Expr> base, index;
};
/// The target type is the enclosing Expr's `type`.
struct Cast {
  CastKind kind;
  bool isImplicit;
  Box<Expr> operand;
};
/// sizeof(type) and sizeof expr. Sema folds both into an IntLiteral.
struct SizeOfType {
  const Type *operand;
};
struct SizeOfExpr {
  Box<Expr> operand;
};

enum class ValueCategory : std::uint8_t {
  RValue, // a plain value, such as 1 + 2
  LValue, // names a memory location, such as x or *p
};

struct Expr {
  using Node =
      std::variant<IntLiteral, FloatLiteral, StringLiteral, DeclRef, Unary,
                   Binary, Assign, Call, Index, Cast, SizeOfType, SizeOfExpr>;
  Node node;
  SourceLocation loc;
  const Type *type = nullptr; // set by Sema (by the parser for casts)
  ValueCategory category = ValueCategory::RValue; // set by Sema

  template <class T> [[nodiscard]] T *as() { return std::get_if<T>(&node); }
  template <class T> [[nodiscard]] const T *as() const {
    return std::get_if<T>(&node);
  }
  [[nodiscard]] bool isLValue() const {
    return category == ValueCategory::LValue;
  }
};

//===----------------------------------------------------------------------===//
// Declarations and statements
//===----------------------------------------------------------------------===//

/// "= 1" or "= {1, 2, {3}}" or "= \"text\"".
struct Initializer {
  std::variant<Expr, std::vector<Initializer>> value;
  SourceLocation loc;
};

struct VarDecl {
  std::string name;
  const Type *type = nullptr;
  bool isConst = false;
  std::optional<Initializer> init;
  SourceLocation loc;
  SymbolId symbol = SymbolId::None;
};

struct Stmt;

struct NullStmt {};
struct ExprStmt {
  Expr expr;
};
struct DeclStmt {
  std::vector<VarDecl> vars;
};
struct CompoundStmt {
  std::vector<Stmt> body;
};
struct IfStmt {
  Expr cond;
  Box<Stmt> then;
  Box<Stmt> otherwise;
}; // else may be null
struct WhileStmt {
  Expr cond;
  Box<Stmt> body;
};
struct BreakStmt {};
struct ContinueStmt {};
struct ReturnStmt {
  std::optional<Expr> value;
};

struct Stmt {
  using Node = std::variant<NullStmt, ExprStmt, DeclStmt, CompoundStmt, IfStmt,
                            WhileStmt, BreakStmt, ContinueStmt, ReturnStmt>;
  Node node;
  SourceLocation loc;
};

struct ParamDecl {
  std::string name;           // empty in prototypes such as "int f(int);"
  const Type *type = nullptr; // already adjusted: arrays become pointers
  bool isConst = false;
  SourceLocation loc;
  SymbolId symbol = SymbolId::None;
};

struct FunctionDecl {
  std::string name;
  const Type *type = nullptr; // a Function type
  std::vector<ParamDecl> params;
  std::optional<CompoundStmt> body; // nullopt for a prototype
  SourceLocation loc;
  SymbolId symbol = SymbolId::None;
};

using TopLevelDecl = std::variant<VarDecl, FunctionDecl>;

/// One source file: its declarations and the types they use.
struct TranslationUnit {
  TypeContext types;
  std::vector<TopLevelDecl> decls;
};

} // namespace jia
