//===- Parser.cpp - Tokens to a syntax tree -------------------------------===//
//
// A recursive-descent parser. Each grammar rule is one method; binary
// operators use precedence climbing. Declarators are parsed the C way:
// first into a list of "derivations" (pointer, array, function), then
// applied to the base type in order. See docs/design/parsing.md.
//
//===----------------------------------------------------------------------===//
#include "jia/Parse/Parser.h"

#include "jia/AST/ConstEval.h"
#include "jia/Lex/Literal.h"

#include <algorithm>
#include <format>
#include <limits>

namespace jia {
namespace {

struct DeclSpec {
  const Type *base;
  bool isConst;
  SourceLocation loc;
};

// One derivation step in a declarator, applied to the type built so far.
struct PointerOp {};
struct ArrayOp {
  std::uint64_t length; // 0 for "[]"
};
struct FunctionOp {
  std::vector<ParamDecl> params;
  bool variadic = false;
};
using DeclOp = std::variant<PointerOp, ArrayOp, FunctionOp>;

/// A parsed but not yet resolved declarator: a name plus derivations in
/// the order they apply to the base type.
struct Declarator {
  std::string name; // empty for abstract declarators such as "int *"
  SourceLocation loc;
  std::vector<DeclOp> ops;
};

/// A declarator applied to its base type.
struct Declared {
  std::string name;
  SourceLocation loc;
  const Type *type;
  std::optional<FunctionOp> function; // set when declaring a function
};

struct BinaryInfo {
  int precedence;
  BinaryOp op;
};

std::optional<BinaryInfo> binaryInfo(TokenKind kind) {
  switch (kind) {
  case TokenKind::PipePipe: return BinaryInfo{1, BinaryOp::LogOr};
  case TokenKind::AmpAmp: return BinaryInfo{2, BinaryOp::LogAnd};
  case TokenKind::EqualEqual: return BinaryInfo{3, BinaryOp::Eq};
  case TokenKind::ExclaimEqual: return BinaryInfo{3, BinaryOp::Ne};
  case TokenKind::Less: return BinaryInfo{4, BinaryOp::Lt};
  case TokenKind::Greater: return BinaryInfo{4, BinaryOp::Gt};
  case TokenKind::LessEqual: return BinaryInfo{4, BinaryOp::Le};
  case TokenKind::GreaterEqual: return BinaryInfo{4, BinaryOp::Ge};
  case TokenKind::Plus: return BinaryInfo{5, BinaryOp::Add};
  case TokenKind::Minus: return BinaryInfo{5, BinaryOp::Sub};
  case TokenKind::Star: return BinaryInfo{6, BinaryOp::Mul};
  case TokenKind::Slash: return BinaryInfo{6, BinaryOp::Div};
  case TokenKind::Percent: return BinaryInfo{6, BinaryOp::Rem};
  default: return std::nullopt;
  }
}

bool isTypeStart(TokenKind kind) {
  return kind == TokenKind::KwVoid || kind == TokenKind::KwChar ||
         kind == TokenKind::KwInt || kind == TokenKind::KwFloat ||
         kind == TokenKind::KwConst;
}

class Parser {
public:
  explicit Parser(std::span<const Token> tokens) : toks_(tokens) {}

  Result<TranslationUnit> run() {
    TranslationUnit tu;
    while (!check(TokenKind::Eof)) {
      const std::size_t before = pos_;
      if (!parseExternalDecl(tu.decls))
        synchronizeTopLevel(before);
    }
    if (!diags_.empty())
      return std::unexpected(std::move(diags_));
    tu.types = std::move(types_);
    return tu;
  }

private:
  //===--- Token helpers ---------------------------------------------===//

  const Token &peek(std::size_t ahead = 0) const {
    return toks_[std::min(pos_ + ahead, toks_.size() - 1)];
  }
  bool check(TokenKind kind) const { return peek().is(kind); }
  const Token &advance() {
    const Token &tok = peek();
    if (!tok.is(TokenKind::Eof))
      ++pos_;
    return tok;
  }
  bool consume(TokenKind kind) {
    if (!check(kind))
      return false;
    advance();
    return true;
  }
  void error(SourceLocation loc, std::string message) {
    diags_.push_back(makeError(loc, std::move(message)));
  }
  bool expect(TokenKind kind) {
    if (consume(kind))
      return true;
    error(peek().loc, std::format("expected '{}' before {}", spelling(kind),
                                  describe(peek())));
    return false;
  }
  static std::string describe(const Token &tok) {
    if (tok.is(TokenKind::Eof))
      return "end of file";
    return std::format("'{}'", tok.text);
  }

  /// Skips to just after the next ';' or '}' at nesting depth zero.
  void synchronizeTopLevel(std::size_t start) {
    if (pos_ == start)
      advance();
    int depth = 0;
    while (!check(TokenKind::Eof)) {
      const TokenKind kind = advance().kind;
      if (kind == TokenKind::LBrace)
        ++depth;
      else if (kind == TokenKind::RBrace && --depth <= 0)
        return;
      else if (kind == TokenKind::Semi && depth == 0)
        return;
    }
  }

  /// Skips to just after the next ';', or to the enclosing '}'.
  void synchronizeStatement(std::size_t start) {
    if (pos_ == start && !check(TokenKind::RBrace))
      advance();
    while (!check(TokenKind::Eof) && !check(TokenKind::RBrace)) {
      if (advance().is(TokenKind::Semi))
        return;
    }
  }

  //===--- Declarations ----------------------------------------------===//

  std::optional<DeclSpec> parseDeclSpec() {
    DeclSpec spec{nullptr, false, peek().loc};
    while (isTypeStart(peek().kind)) {
      const Token &tok = advance();
      const Type *type = nullptr;
      switch (tok.kind) {
      case TokenKind::KwConst: spec.isConst = true; continue;
      case TokenKind::KwVoid: type = types_.voidType(); break;
      case TokenKind::KwChar: type = types_.charType(); break;
      case TokenKind::KwInt: type = types_.intType(); break;
      case TokenKind::KwFloat: type = types_.floatType(); break;
      default: break;
      }
      if (spec.base) {
        error(tok.loc, "cannot combine type specifiers");
        return std::nullopt;
      }
      spec.base = type;
    }
    if (!spec.base) {
      error(peek().loc, std::format("expected a type before {}", describe(peek())));
      return std::nullopt;
    }
    return spec;
  }

  /// Parses a declarator. In \p abstract mode the name is optional, as in
  /// casts ("(int *)") and prototype parameters ("int f(char *)").
  std::optional<Declarator> parseDeclarator(bool abstract) {
    Declarator decl{"", peek().loc, {}};
    int stars = 0;
    while (consume(TokenKind::Star)) {
      ++stars;
      consume(TokenKind::KwConst); // "* const" is accepted and ignored
    }

    std::optional<Declarator> inner;
    const bool nested =
        check(TokenKind::LParen) &&
        (!abstract || peek(1).is(TokenKind::Star) ||
         peek(1).is(TokenKind::Identifier));
    if (nested) {
      advance();
      inner = parseDeclarator(abstract);
      if (!inner || !expect(TokenKind::RParen))
        return std::nullopt;
    } else if (check(TokenKind::Identifier)) {
      decl.loc = peek().loc;
      decl.name = std::string(advance().text);
    } else if (!abstract) {
      error(peek().loc, std::format("expected identifier before {}", describe(peek())));
      return std::nullopt;
    }

    std::vector<DeclOp> suffixes;
    while (true) {
      if (consume(TokenKind::LSquare)) {
        auto length = parseArrayLength();
        if (!length)
          return std::nullopt;
        suffixes.emplace_back(ArrayOp{*length});
      } else if (consume(TokenKind::LParen)) {
        auto fn = parseParams();
        if (!fn)
          return std::nullopt;
        suffixes.emplace_back(std::move(*fn));
      } else {
        break;
      }
    }

    for (int i = 0; i < stars; ++i)
      decl.ops.emplace_back(PointerOp{});
    std::move(suffixes.rbegin(), suffixes.rend(), std::back_inserter(decl.ops));
    if (inner) {
      decl.name = std::move(inner->name);
      decl.loc = inner->loc;
      std::move(inner->ops.begin(), inner->ops.end(), std::back_inserter(decl.ops));
    }
    return decl;
  }

  /// Parses what follows "[": an optional constant size, then "]".
  std::optional<std::uint64_t> parseArrayLength() {
    if (consume(TokenKind::RSquare))
      return 0;
    const SourceLocation loc = peek().loc;
    auto size = parseExpr();
    if (!size || !expect(TokenKind::RSquare))
      return std::nullopt;
    const auto value = evaluateInteger(*size);
    if (!value) {
      error(loc, "array size must be an integer constant");
      return std::nullopt;
    }
    if (*value <= 0) {
      error(loc, "array size must be positive");
      return std::nullopt;
    }
    return static_cast<std::uint64_t>(*value);
  }

  /// Parses a parameter list after "(", through ")".
  std::optional<FunctionOp> parseParams() {
    FunctionOp fn;
    if (consume(TokenKind::RParen))
      return fn;
    if (check(TokenKind::KwVoid) && peek(1).is(TokenKind::RParen)) {
      advance();
      advance();
      return fn;
    }
    do {
      if (consume(TokenKind::Ellipsis)) {
        fn.variadic = true;
        break;
      }
      auto spec = parseDeclSpec();
      if (!spec)
        return std::nullopt;
      auto decl = parseDeclarator(/*abstract=*/true);
      if (!decl)
        return std::nullopt;
      auto declared = resolve(*spec, std::move(*decl));
      if (!declared)
        return std::nullopt;
      const Type *type = declared->type;
      if (type->isFunction()) {
        error(declared->loc, "function parameters are not supported");
        return std::nullopt;
      }
      if (type->isVoid()) {
        error(declared->loc, "parameter has type 'void'");
        return std::nullopt;
      }
      if (type->isArray()) // C adjusts "T a[]" in parameters to "T *a"
        type = types_.pointerTo(type->element());
      fn.params.push_back(ParamDecl{.name = std::move(declared->name),
                                    .type = type,
                                    .isConst = spec->isConst,
                                    .loc = declared->loc});
    } while (consume(TokenKind::Comma));
    if (!expect(TokenKind::RParen))
      return std::nullopt;
    return fn;
  }

  /// Applies a declarator's derivations to the base type.
  std::optional<Declared> resolve(const DeclSpec &spec, Declarator decl) {
    Declared out{std::move(decl.name), decl.loc, spec.base, std::nullopt};
    for (std::size_t i = 0; i < decl.ops.size(); ++i) {
      const Type *t = out.type;
      bool ok = std::visit(
          [&](auto &op) {
            using Op = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<Op, PointerOp>) {
              out.type = types_.pointerTo(t);
            } else if constexpr (std::is_same_v<Op, ArrayOp>) {
              if (t->isVoid() || t->isFunction() ||
                  (t->isArray() && t->length() == 0)) {
                error(out.loc, std::format("array has invalid element type '{}'",
                                           t->str()));
                return false;
              }
              out.type = types_.arrayOf(t, op.length);
            } else {
              if (t->isArray() || t->isFunction()) {
                error(out.loc, std::format("function cannot return '{}'", t->str()));
                return false;
              }
              if (i + 1 != decl.ops.size()) {
                error(out.loc, "function pointers are not supported yet");
                return false;
              }
              std::vector<const Type *> params;
              for (const ParamDecl &p : op.params)
                params.push_back(p.type);
              out.type = types_.function(t, std::move(params), op.variadic);
              out.function = std::move(op);
            }
            return true;
          },
          decl.ops[i]);
      if (!ok)
        return std::nullopt;
    }
    return out;
  }

  std::optional<const Type *> parseTypeName() {
    auto spec = parseDeclSpec();
    if (!spec)
      return std::nullopt;
    auto decl = parseDeclarator(/*abstract=*/true);
    if (!decl)
      return std::nullopt;
    const SourceLocation loc = decl->loc;
    if (!decl->name.empty()) {
      error(loc, "unexpected name in type");
      return std::nullopt;
    }
    auto declared = resolve(*spec, std::move(*decl));
    if (!declared)
      return std::nullopt;
    if (declared->type->isFunction()) {
      error(loc, "function types are not supported here");
      return std::nullopt;
    }
    return declared->type;
  }

  /// external-declaration: a function definition, or a declaration list.
  bool parseExternalDecl(std::vector<TopLevelDecl> &out) {
    auto spec = parseDeclSpec();
    if (!spec)
      return false;
    if (consume(TokenKind::Semi))
      return true; // "int;" declares nothing
    auto decl = parseDeclarator(/*abstract=*/false);
    if (!decl)
      return false;
    auto first = resolve(*spec, std::move(*decl));
    if (!first)
      return false;

    if (first->function && check(TokenKind::LBrace)) {
      auto body = parseCompound();
      if (!body)
        return false;
      out.emplace_back(FunctionDecl{.name = std::move(first->name),
                                    .type = first->type,
                                    .params = std::move(first->function->params),
                                    .body = std::move(*body),
                                    .loc = first->loc});
      return true;
    }

    std::optional<Declared> current = std::move(first);
    while (true) {
      if (current->function) {
        out.emplace_back(FunctionDecl{.name = std::move(current->name),
                                      .type = current->type,
                                      .params = std::move(current->function->params),
                                      .loc = current->loc});
      } else {
        auto var = finishVarDecl(*spec, std::move(*current));
        if (!var)
          return false;
        out.emplace_back(std::move(*var));
      }
      if (!consume(TokenKind::Comma))
        break;
      auto next = parseDeclarator(/*abstract=*/false);
      if (!next)
        return false;
      current = resolve(*spec, std::move(*next));
      if (!current)
        return false;
    }
    return expect(TokenKind::Semi);
  }

  /// Builds a VarDecl and parses its optional "= initializer".
  std::optional<VarDecl> finishVarDecl(const DeclSpec &spec, Declared declared) {
    if (declared.type->isVoid()) {
      error(declared.loc, std::format("variable '{}' has type 'void'", declared.name));
      return std::nullopt;
    }
    VarDecl var{.name = std::move(declared.name),
                .type = declared.type,
                .isConst = spec.isConst,
                .loc = declared.loc};
    if (consume(TokenKind::Equal)) {
      auto init = parseInitializer();
      if (!init)
        return std::nullopt;
      var.init = std::move(*init);
    }
    return var;
  }

  std::optional<Initializer> parseInitializer() {
    const SourceLocation loc = peek().loc;
    if (!consume(TokenKind::LBrace)) {
      auto expr = parseAssignment();
      if (!expr)
        return std::nullopt;
      return Initializer{std::move(*expr), loc};
    }
    std::vector<Initializer> items;
    while (!check(TokenKind::RBrace)) {
      auto item = parseInitializer();
      if (!item)
        return std::nullopt;
      items.push_back(std::move(*item));
      if (!consume(TokenKind::Comma))
        break;
    }
    if (!expect(TokenKind::RBrace))
      return std::nullopt;
    return Initializer{std::move(items), loc};
  }

  //===--- Statements ------------------------------------------------===//

  std::optional<CompoundStmt> parseCompound() {
    if (!expect(TokenKind::LBrace))
      return std::nullopt;
    // A broken statement is already reported; keep the rest of the block
    // so that recovery does not swallow the next declaration.
    CompoundStmt block;
    while (!check(TokenKind::RBrace) && !check(TokenKind::Eof)) {
      const std::size_t before = pos_;
      if (auto stmt = parseStatement()) {
        block.body.push_back(std::move(*stmt));
      } else {
        synchronizeStatement(before);
      }
    }
    if (!expect(TokenKind::RBrace))
      return std::nullopt;
    return block;
  }

  std::optional<Stmt> parseStatement() {
    const SourceLocation loc = peek().loc;
    const auto make = [&](Stmt::Node node) {
      return std::optional<Stmt>(Stmt{std::move(node), loc});
    };

    switch (peek().kind) {
    case TokenKind::LBrace: {
      auto block = parseCompound();
      if (!block)
        return std::nullopt;
      return make(std::move(*block));
    }
    case TokenKind::Semi:
      advance();
      return make(NullStmt{});
    case TokenKind::KwBreak:
      advance();
      if (!expect(TokenKind::Semi))
        return std::nullopt;
      return make(BreakStmt{});
    case TokenKind::KwContinue:
      advance();
      if (!expect(TokenKind::Semi))
        return std::nullopt;
      return make(ContinueStmt{});
    case TokenKind::KwReturn: {
      advance();
      ReturnStmt ret;
      if (!check(TokenKind::Semi)) {
        auto value = parseExpr();
        if (!value)
          return std::nullopt;
        ret.value = std::move(*value);
      }
      if (!expect(TokenKind::Semi))
        return std::nullopt;
      return make(std::move(ret));
    }
    case TokenKind::KwIf: {
      advance();
      auto cond = parseCondition();
      if (!cond)
        return std::nullopt;
      auto then = parseStatement();
      if (!then)
        return std::nullopt;
      Box<Stmt> otherwise;
      if (consume(TokenKind::KwElse)) {
        auto elseStmt = parseStatement();
        if (!elseStmt)
          return std::nullopt;
        otherwise = box(std::move(*elseStmt));
      }
      return make(IfStmt{std::move(*cond), box(std::move(*then)),
                         std::move(otherwise)});
    }
    case TokenKind::KwWhile: {
      advance();
      auto cond = parseCondition();
      if (!cond)
        return std::nullopt;
      auto body = parseStatement();
      if (!body)
        return std::nullopt;
      return make(WhileStmt{std::move(*cond), box(std::move(*body))});
    }
    default:
      break;
    }

    if (isTypeStart(peek().kind))
      return parseDeclStmt();

    auto expr = parseExpr();
    if (!expr || !expect(TokenKind::Semi))
      return std::nullopt;
    return make(ExprStmt{std::move(*expr)});
  }

  /// "( expression )" after if or while.
  std::optional<Expr> parseCondition() {
    if (!expect(TokenKind::LParen))
      return std::nullopt;
    auto cond = parseExpr();
    if (!cond || !expect(TokenKind::RParen))
      return std::nullopt;
    return cond;
  }

  std::optional<Stmt> parseDeclStmt() {
    const SourceLocation loc = peek().loc;
    auto spec = parseDeclSpec();
    if (!spec)
      return std::nullopt;
    DeclStmt stmt;
    do {
      auto decl = parseDeclarator(/*abstract=*/false);
      if (!decl)
        return std::nullopt;
      auto declared = resolve(*spec, std::move(*decl));
      if (!declared)
        return std::nullopt;
      if (declared->function) {
        error(declared->loc, "functions must be declared at file scope");
        return std::nullopt;
      }
      auto var = finishVarDecl(*spec, std::move(*declared));
      if (!var)
        return std::nullopt;
      stmt.vars.push_back(std::move(*var));
    } while (consume(TokenKind::Comma));
    if (!expect(TokenKind::Semi))
      return std::nullopt;
    return Stmt{std::move(stmt), loc};
  }

  //===--- Expressions -----------------------------------------------===//

  std::optional<Expr> parseExpr() { return parseAssignment(); }

  std::optional<Expr> parseAssignment() {
    auto lhs = parseBinary(1);
    if (!lhs || !check(TokenKind::Equal))
      return lhs;
    const SourceLocation loc = advance().loc;
    auto rhs = parseAssignment(); // right-associative: a = b = c
    if (!rhs)
      return std::nullopt;
    return Expr{.node = Assign{box(std::move(*lhs)), box(std::move(*rhs))},
                .loc = loc};
  }

  /// Precedence climbing: parses operators that bind at least as tightly
  /// as \p minPrecedence. All binary operators are left-associative.
  std::optional<Expr> parseBinary(int minPrecedence) {
    auto lhs = parseUnary();
    if (!lhs)
      return std::nullopt;
    while (const auto info = binaryInfo(peek().kind)) {
      if (info->precedence < minPrecedence)
        break;
      const SourceLocation loc = advance().loc;
      auto rhs = parseBinary(info->precedence + 1);
      if (!rhs)
        return std::nullopt;
      lhs = Expr{.node = Binary{info->op, box(std::move(*lhs)),
                                box(std::move(*rhs))},
                 .loc = loc};
    }
    return lhs;
  }

  std::optional<Expr> parseUnary() {
    const SourceLocation loc = peek().loc;
    std::optional<UnaryOp> op;
    switch (peek().kind) {
    case TokenKind::Plus: op = UnaryOp::Plus; break;
    case TokenKind::Minus: op = UnaryOp::Minus; break;
    case TokenKind::Exclaim: op = UnaryOp::Not; break;
    case TokenKind::Star: op = UnaryOp::Deref; break;
    case TokenKind::Amp: op = UnaryOp::AddrOf; break;
    default: break;
    }
    if (op) {
      advance();
      auto operand = parseUnary();
      if (!operand)
        return std::nullopt;
      return Expr{.node = Unary{*op, box(std::move(*operand))}, .loc = loc};
    }

    if (consume(TokenKind::KwSizeof)) {
      if (check(TokenKind::LParen) && isTypeStart(peek(1).kind)) {
        advance();
        auto type = parseTypeName();
        if (!type || !expect(TokenKind::RParen))
          return std::nullopt;
        return Expr{.node = SizeOfType{*type}, .loc = loc};
      }
      auto operand = parseUnary();
      if (!operand)
        return std::nullopt;
      return Expr{.node = SizeOfExpr{box(std::move(*operand))}, .loc = loc};
    }

    if (check(TokenKind::LParen) && isTypeStart(peek(1).kind)) {
      advance();
      auto type = parseTypeName();
      if (!type || !expect(TokenKind::RParen))
        return std::nullopt;
      auto operand = parseUnary();
      if (!operand)
        return std::nullopt;
      return Expr{.node = Cast{CastKind::Unresolved, false, box(std::move(*operand))},
                  .loc = loc,
                  .type = *type};
    }
    return parsePostfix();
  }

  std::optional<Expr> parsePostfix() {
    auto expr = parsePrimary();
    if (!expr)
      return std::nullopt;
    while (true) {
      const SourceLocation loc = peek().loc;
      if (consume(TokenKind::LParen)) {
        std::vector<Expr> args;
        if (!check(TokenKind::RParen)) {
          do {
            auto arg = parseAssignment();
            if (!arg)
              return std::nullopt;
            args.push_back(std::move(*arg));
          } while (consume(TokenKind::Comma));
        }
        if (!expect(TokenKind::RParen))
          return std::nullopt;
        const SourceLocation calleeLoc = expr->loc;
        expr = Expr{.node = Call{box(std::move(*expr)), std::move(args)},
                    .loc = calleeLoc};
      } else if (consume(TokenKind::LSquare)) {
        auto index = parseExpr();
        if (!index || !expect(TokenKind::RSquare))
          return std::nullopt;
        expr = Expr{.node = Index{box(std::move(*expr)), box(std::move(*index))},
                    .loc = loc};
      } else {
        return expr;
      }
    }
  }

  std::optional<Expr> parsePrimary() {
    const Token &tok = peek();
    const SourceLocation loc = tok.loc;
    switch (tok.kind) {
    case TokenKind::Identifier:
      advance();
      return Expr{.node = DeclRef{std::string(tok.text)}, .loc = loc};
    case TokenKind::IntLiteral: {
      advance();
      const auto value = parseIntLiteral(tok.text);
      if (!value) {
        error(loc, value.error());
        return std::nullopt;
      }
      if (*value > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
        error(loc, "integer literal is too large for type 'int'");
        return std::nullopt;
      }
      return Expr{.node = IntLiteral{static_cast<std::int64_t>(*value)}, .loc = loc};
    }
    case TokenKind::FloatLiteral: {
      advance();
      const auto value = parseFloatLiteral(tok.text);
      if (!value) {
        error(loc, value.error());
        return std::nullopt;
      }
      return Expr{.node = FloatLiteral{*value}, .loc = loc};
    }
    case TokenKind::CharLiteral: {
      advance();
      const auto value = parseCharLiteral(tok.text);
      if (!value) {
        error(loc, value.error());
        return std::nullopt;
      }
      // As in C, a character constant has type int.
      return Expr{.node = IntLiteral{static_cast<signed char>(*value)}, .loc = loc};
    }
    case TokenKind::StringLiteral: {
      std::string bytes;
      while (check(TokenKind::StringLiteral)) { // "a" "b" is "ab"
        const Token &part = advance();
        const auto decoded = parseStringLiteral(part.text);
        if (!decoded) {
          error(part.loc, decoded.error());
          return std::nullopt;
        }
        bytes += *decoded;
      }
      return Expr{.node = StringLiteral{std::move(bytes)}, .loc = loc};
    }
    case TokenKind::LParen: {
      advance();
      auto inner = parseExpr();
      if (!inner || !expect(TokenKind::RParen))
        return std::nullopt;
      return inner;
    }
    default:
      error(loc, std::format("expected expression before {}", describe(tok)));
      return std::nullopt;
    }
  }

  std::span<const Token> toks_;
  std::size_t pos_ = 0;
  TypeContext types_;
  Diagnostics diags_;
};

} // namespace

Result<TranslationUnit> parse(std::span<const Token> tokens) {
  return Parser(tokens).run();
}

} // namespace jia
