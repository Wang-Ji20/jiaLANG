//===- CodeGen.cpp - Typed tree to LLVM IR --------------------------------===//
//
// Every local variable lives in a stack slot (an alloca in the entry
// block); every read is a load and every write a store. LLVM's mem2reg pass
// later promotes the slots to registers. This keeps code generation simple
// without costing performance. See docs/design/codegen.md.
//
//===----------------------------------------------------------------------===//
#include "jia/CodeGen/CodeGen.h"

#include "jia/AST/ConstEval.h"
#include "jia/Basic/Overloaded.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <map>

namespace jia {
namespace {

class ModuleBuilder {
public:
  ModuleBuilder(llvm::LLVMContext &ctx, std::string_view name)
      : ctx_(ctx), module_(std::make_unique<llvm::Module>(name, ctx)), b_(ctx) {
  }

  Result<std::unique_ptr<llvm::Module>> run(const TranslationUnit &tu) {
    for (const TopLevelDecl &decl : tu.decls)
      std::visit(Overloaded{
                     [&](const VarDecl &var) { emitGlobal(var); },
                     [&](const FunctionDecl &fn) { emitFunction(fn); },
                 },
                 decl);
    std::string problems;
    llvm::raw_string_ostream os(problems);
    if (llvm::verifyModule(*module_, &os))
      return fail({}, "internal compiler error: invalid LLVM IR:\n" + problems);
    return std::move(module_);
  }

private:
  //===--- Types -----------------------------------------------------===//

  llvm::Type *lower(const Type *type) {
    switch (type->kind()) {
    case TypeKind::Void:
      return b_.getVoidTy();
    case TypeKind::Char:
      return b_.getInt8Ty();
    case TypeKind::Int:
      return b_.getInt32Ty();
    case TypeKind::Float:
      return b_.getFloatTy();
    case TypeKind::Pointer:
      return b_.getPtrTy();
    case TypeKind::Array:
      return llvm::ArrayType::get(lower(type->element()), type->length());
    case TypeKind::Function:
      return lowerFunction(type);
    }
    return nullptr;
  }

  llvm::FunctionType *lowerFunction(const Type *type) {
    std::vector<llvm::Type *> params;
    for (const Type *param : type->params())
      params.push_back(lower(param));
    return llvm::FunctionType::get(lower(type->returnType()), params,
                                   type->isVariadic());
  }

  //===--- Globals and functions -------------------------------------===//

  void emitGlobal(const VarDecl &var) {
    llvm::Type *type = lower(var.type);
    llvm::Constant *init = var.init ? constantInit(var.type, *var.init)
                                    : llvm::Constant::getNullValue(type);
    // The module owns the global; this is the LLVM-API exception to our
    // no-raw-new rule.
    auto *global = new llvm::GlobalVariable(*module_, type, var.isConst,
                                            llvm::GlobalValue::ExternalLinkage,
                                            init, var.name);
    values_[var.symbol] = global;
  }

  llvm::Function *function(const FunctionDecl &fn) {
    if (auto it = values_.find(fn.symbol); it != values_.end())
      return llvm::cast<llvm::Function>(it->second);
    auto *f = llvm::Function::Create(lowerFunction(fn.type),
                                     llvm::GlobalValue::ExternalLinkage,
                                     fn.name, *module_);
    values_[fn.symbol] = f;
    return f;
  }

  void emitFunction(const FunctionDecl &decl) {
    llvm::Function *f = function(decl);
    if (!decl.body)
      return;
    fn_ = f;
    b_.SetInsertPoint(llvm::BasicBlock::Create(ctx_, "entry", f));
    for (auto [arg, param] : llvm::zip(f->args(), decl.params)) {
      arg.setName(param.name);
      llvm::AllocaInst *slot = stackSlot(param.type, param.name);
      b_.CreateStore(&arg, slot);
      values_[param.symbol] = slot;
    }
    for (const Stmt &stmt : decl.body->body)
      emit(stmt);
    if (!b_.GetInsertBlock()->hasTerminator()) {
      // Falling off the end: C returns garbage, we return zero (and main
      // returning 0 is what C requires).
      llvm::Type *ret = f->getReturnType();
      if (ret->isVoidTy())
        b_.CreateRetVoid();
      else
        b_.CreateRet(llvm::Constant::getNullValue(ret));
    }
  }

  /// A stack slot in the entry block, so mem2reg can promote it.
  llvm::AllocaInst *stackSlot(const Type *type, std::string_view name) {
    llvm::BasicBlock &entry = fn_->getEntryBlock();
    llvm::IRBuilder<> atEntry(&entry, entry.begin());
    return atEntry.CreateAlloca(lower(type), nullptr, name);
  }

  //===--- Statements ------------------------------------------------===//

  llvm::BasicBlock *block(std::string_view name) {
    return llvm::BasicBlock::Create(ctx_, name);
  }

  void startBlock(llvm::BasicBlock *bb) {
    bb->insertInto(fn_);
    b_.SetInsertPoint(bb);
  }

  void branchTo(llvm::BasicBlock *target) {
    if (!b_.GetInsertBlock()->hasTerminator())
      b_.CreateBr(target);
  }

  /// After return, break, or continue, later code is unreachable; give it
  /// a fresh block so every block still ends in exactly one terminator.
  void startDeadBlock() { startBlock(block("dead")); }

  void emit(const Stmt &stmt) {
    std::visit(Overloaded{
                   [](const NullStmt &) {},
                   [&](const ExprStmt &s) { rvalue(s.expr); },
                   [&](const DeclStmt &s) {
                     for (const VarDecl &var : s.vars) {
                       llvm::AllocaInst *slot = stackSlot(var.type, var.name);
                       values_[var.symbol] = slot;
                       if (var.init)
                         localInit(slot, var.type, *var.init);
                     }
                   },
                   [&](const CompoundStmt &s) {
                     for (const Stmt &inner : s.body)
                       emit(inner);
                   },
                   [&](const IfStmt &s) {
                     llvm::BasicBlock *thenBB = block("if.then");
                     llvm::BasicBlock *elseBB =
                         s.otherwise ? block("if.else") : nullptr;
                     llvm::BasicBlock *endBB = block("if.end");
                     b_.CreateCondBr(condition(s.cond), thenBB,
                                     elseBB ? elseBB : endBB);
                     startBlock(thenBB);
                     emit(*s.then);
                     branchTo(endBB);
                     if (elseBB) {
                       startBlock(elseBB);
                       emit(*s.otherwise);
                       branchTo(endBB);
                     }
                     startBlock(endBB);
                   },
                   [&](const WhileStmt &s) {
                     llvm::BasicBlock *condBB = block("while.cond");
                     llvm::BasicBlock *bodyBB = block("while.body");
                     llvm::BasicBlock *endBB = block("while.end");
                     b_.CreateBr(condBB);
                     startBlock(condBB);
                     b_.CreateCondBr(condition(s.cond), bodyBB, endBB);
                     startBlock(bodyBB);
                     loops_.push_back({.continueTo = condBB, .breakTo = endBB});
                     emit(*s.body);
                     loops_.pop_back();
                     branchTo(condBB);
                     startBlock(endBB);
                   },
                   [&](const BreakStmt &) {
                     b_.CreateBr(loops_.back().breakTo);
                     startDeadBlock();
                   },
                   [&](const ContinueStmt &) {
                     b_.CreateBr(loops_.back().continueTo);
                     startDeadBlock();
                   },
                   [&](const ReturnStmt &s) {
                     if (s.value && !s.value->type->isVoid())
                       b_.CreateRet(rvalue(*s.value));
                     else {
                       if (s.value)
                         rvalue(*s.value); // "return f();" where f returns void
                       b_.CreateRetVoid();
                     }
                     startDeadBlock();
                   },
               },
               stmt.node);
  }

  //===--- Initializers ----------------------------------------------===//

  /// A char array holding \p bytes, padded with NULs (or truncated, when
  /// C lets the terminator drop) to the array's length.
  llvm::Constant *charArray(const Type *arrayType, std::string bytes) {
    bytes.resize(arrayType->length(), '\0');
    return llvm::ConstantDataArray::getString(ctx_, bytes, /*AddNull=*/false);
  }

  void localInit(llvm::Value *addr, const Type *type, const Initializer &init) {
    const auto *list = std::get_if<std::vector<Initializer>>(&init.value);
    const auto *expr = std::get_if<Expr>(&init.value);
    if (type->isArray()) {
      if (expr) { // a string literal, checked by Sema
        b_.CreateStore(charArray(type, expr->as<StringLiteral>()->bytes), addr);
        return;
      }
      llvm::Type *arrayType = lower(type);
      b_.CreateStore(llvm::Constant::getNullValue(arrayType), addr);
      for (std::size_t i = 0; i < list->size(); ++i)
        localInit(b_.CreateConstInBoundsGEP2_64(arrayType, addr, 0, i),
                  type->element(), (*list)[i]);
      return;
    }
    if (list) {
      localInit(addr, type, list->front());
      return;
    }
    b_.CreateStore(rvalue(*expr), addr);
  }

  llvm::Constant *constantInit(const Type *type, const Initializer &init) {
    const auto *list = std::get_if<std::vector<Initializer>>(&init.value);
    const auto *expr = std::get_if<Expr>(&init.value);
    if (type->isArray()) {
      if (expr)
        return charArray(type, expr->as<StringLiteral>()->bytes);
      std::vector<llvm::Constant *> elements;
      for (std::uint64_t i = 0; i < type->length(); ++i)
        elements.push_back(
            i < list->size()
                ? constantInit(type->element(), (*list)[i])
                : llvm::Constant::getNullValue(lower(type->element())));
      return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(lower(type)),
                                      elements);
    }
    if (list)
      return constantInit(type, list->front());
    return constant(*expr);
  }

  /// Builds a constant for an expression Sema accepted as a constant
  /// initializer.
  llvm::Constant *constant(const Expr &e) {
    if (const auto value = evaluateConstant(e)) {
      llvm::Type *type = lower(e.type);
      return std::visit(Overloaded{
                            [&](std::int64_t i) -> llvm::Constant * {
                              if (type->isFloatTy())
                                return llvm::ConstantFP::get(
                                    type, static_cast<double>(i));
                              return llvm::ConstantInt::getSigned(type, i);
                            },
                            [&](double d) -> llvm::Constant * {
                              if (type->isFloatTy())
                                return llvm::ConstantFP::get(type, d);
                              return llvm::ConstantInt::getSigned(
                                  type, static_cast<std::int64_t>(d));
                            },
                        },
                        *value);
    }
    if (const auto *cast = e.as<Cast>()) {
      if (cast->kind == CastKind::NullToPointer)
        return llvm::ConstantPointerNull::get(b_.getPtrTy());
      if (cast->kind == CastKind::PointerCast)
        return constant(*cast->operand);
      if (cast->kind == CastKind::ArrayToPointer)
        return llvm::cast<llvm::Constant>(lvalue(*cast->operand));
    }
    if (const auto *unary = e.as<Unary>();
        unary && unary->op == UnaryOp::AddrOf)
      return llvm::cast<llvm::Constant>(lvalue(*unary->operand));
    assert(false && "Sema accepted a non-constant initializer");
    return nullptr;
  }

  //===--- Expressions -----------------------------------------------===//

  llvm::GlobalVariable *stringLiteral(const std::string &bytes) {
    auto [it, inserted] = strings_.try_emplace(bytes, nullptr);
    if (inserted) {
      auto *data =
          llvm::ConstantDataArray::getString(ctx_, bytes, /*AddNull=*/true);
      it->second = new llvm::GlobalVariable(*module_, data->getType(), true,
                                            llvm::GlobalValue::PrivateLinkage,
                                            data, ".str");
      it->second->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    }
    return it->second;
  }

  llvm::Value *valueOf(SymbolId symbol) const {
    const auto it = values_.find(symbol);
    assert(it != values_.end() && "Sema resolved a name codegen never saw");
    return it->second;
  }

  /// The address an lvalue expression names.
  llvm::Value *lvalue(const Expr &e) {
    if (const auto *ref = e.as<DeclRef>())
      return valueOf(ref->symbol);
    if (const auto *unary = e.as<Unary>(); unary && unary->op == UnaryOp::Deref)
      return rvalue(*unary->operand);
    if (const auto *str = e.as<StringLiteral>())
      return stringLiteral(str->bytes);
    assert(false && "not an lvalue");
    return nullptr;
  }

  /// An i1 that is true when the scalar \p e is nonzero.
  llvm::Value *condition(const Expr &e) {
    llvm::Value *v = rvalue(e);
    if (e.type->is(TypeKind::Float))
      return b_.CreateFCmpUNE(v, llvm::ConstantFP::get(v->getType(), 0.0));
    return b_.CreateIsNotNull(v);
  }

  llvm::Value *toInt(llvm::Value *flag) {
    return b_.CreateZExt(flag, b_.getInt32Ty());
  }

  llvm::Value *rvalue(const Expr &e) {
    return std::visit(
        Overloaded{
            [&](const IntLiteral &lit) -> llvm::Value * {
              return llvm::ConstantInt::getSigned(lower(e.type), lit.value);
            },
            [&](const FloatLiteral &lit) -> llvm::Value * {
              return llvm::ConstantFP::get(lower(e.type), lit.value);
            },
            [&](const StringLiteral &) { return lvalue(e); },
            [&](const DeclRef &) -> llvm::Value * {
              return b_.CreateLoad(lower(e.type), lvalue(e));
            },
            [&](const Unary &u) { return unary(e, u); },
            [&](const Binary &bin) { return binary(bin); },
            [&](const Assign &a) -> llvm::Value * {
              llvm::Value *value = rvalue(*a.value);
              b_.CreateStore(value, lvalue(*a.target));
              return value;
            },
            [&](const Call &c) { return call(c); },
            [&](const Cast &c) { return cast(e, c); },
            [](const Index &) -> llvm::Value * { return nullptr; },
            [](const SizeOfType &) -> llvm::Value * { return nullptr; },
            [](const SizeOfExpr &) -> llvm::Value * { return nullptr; },
        },
        e.node);
  }

  llvm::Value *unary(const Expr &e, const Unary &u) {
    switch (u.op) {
    case UnaryOp::Plus:
      return rvalue(*u.operand);
    case UnaryOp::Minus: {
      llvm::Value *v = rvalue(*u.operand);
      return e.type->is(TypeKind::Float) ? b_.CreateFNeg(v) : b_.CreateNeg(v);
    }
    case UnaryOp::Not:
      return toInt(b_.CreateNot(condition(*u.operand)));
    case UnaryOp::Deref:
      return b_.CreateLoad(lower(e.type), rvalue(*u.operand));
    case UnaryOp::AddrOf:
      return lvalue(*u.operand);
    }
    return nullptr;
  }

  /// && and || evaluate their right side only when needed.
  llvm::Value *logical(const Binary &bin) {
    const bool isAnd = bin.op == BinaryOp::LogAnd;
    llvm::Value *lhs = condition(*bin.lhs);
    llvm::BasicBlock *lhsEnd = b_.GetInsertBlock();
    llvm::BasicBlock *rhsBB = block(isAnd ? "land.rhs" : "lor.rhs");
    llvm::BasicBlock *endBB = block(isAnd ? "land.end" : "lor.end");
    if (isAnd)
      b_.CreateCondBr(lhs, rhsBB, endBB);
    else
      b_.CreateCondBr(lhs, endBB, rhsBB);
    startBlock(rhsBB);
    llvm::Value *rhs = condition(*bin.rhs);
    llvm::BasicBlock *rhsEnd = b_.GetInsertBlock();
    b_.CreateBr(endBB);
    startBlock(endBB);
    llvm::PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), 2);
    phi->addIncoming(b_.getInt1(!isAnd), lhsEnd);
    phi->addIncoming(rhs, rhsEnd);
    return toInt(phi);
  }

  llvm::Value *binary(const Binary &bin) {
    if (bin.op == BinaryOp::LogAnd || bin.op == BinaryOp::LogOr)
      return logical(bin);

    const Type *operandType = bin.lhs->type;
    llvm::Value *l = rvalue(*bin.lhs);
    llvm::Value *r = rvalue(*bin.rhs);
    const bool isFloat = operandType->is(TypeKind::Float);

    if (operandType->isPointer()) {
      llvm::Type *element = lower(operandType->pointee());
      if (bin.op == BinaryOp::Add ||
          (bin.op == BinaryOp::Sub && !bin.rhs->type->isPointer())) {
        llvm::Value *offset = b_.CreateSExt(r, b_.getInt64Ty());
        if (bin.op == BinaryOp::Sub)
          offset = b_.CreateNeg(offset);
        return b_.CreateInBoundsGEP(element, l, offset);
      }
      if (bin.op == BinaryOp::Sub)
        return b_.CreateTrunc(b_.CreatePtrDiff(element, l, r), b_.getInt32Ty());
    }

    switch (bin.op) {
    case BinaryOp::Add:
      return isFloat ? b_.CreateFAdd(l, r) : b_.CreateAdd(l, r);
    case BinaryOp::Sub:
      return isFloat ? b_.CreateFSub(l, r) : b_.CreateSub(l, r);
    case BinaryOp::Mul:
      return isFloat ? b_.CreateFMul(l, r) : b_.CreateMul(l, r);
    case BinaryOp::Div:
      return isFloat ? b_.CreateFDiv(l, r) : b_.CreateSDiv(l, r);
    case BinaryOp::Rem:
      return b_.CreateSRem(l, r);
    default:
      break;
    }

    // Comparisons yield an int, 0 or 1, as in C.
    using P = llvm::CmpInst::Predicate;
    const bool isPtr = operandType->isPointer();
    P pred = P::ICMP_EQ;
    switch (bin.op) {
    case BinaryOp::Lt:
      pred = isFloat ? P::FCMP_OLT : isPtr ? P::ICMP_ULT : P::ICMP_SLT;
      break;
    case BinaryOp::Gt:
      pred = isFloat ? P::FCMP_OGT : isPtr ? P::ICMP_UGT : P::ICMP_SGT;
      break;
    case BinaryOp::Le:
      pred = isFloat ? P::FCMP_OLE : isPtr ? P::ICMP_ULE : P::ICMP_SLE;
      break;
    case BinaryOp::Ge:
      pred = isFloat ? P::FCMP_OGE : isPtr ? P::ICMP_UGE : P::ICMP_SGE;
      break;
    case BinaryOp::Eq:
      pred = isFloat ? P::FCMP_OEQ : P::ICMP_EQ;
      break;
    case BinaryOp::Ne:
      pred = isFloat ? P::FCMP_UNE : P::ICMP_NE;
      break; // NaN != NaN
    default:
      break;
    }
    return toInt(b_.CreateCmp(pred, l, r));
  }

  llvm::Value *call(const Call &c) {
    llvm::Function *f =
        llvm::cast<llvm::Function>(valueOf(c.callee->as<DeclRef>()->symbol));
    const std::size_t fixed = f->getFunctionType()->getNumParams();
    std::vector<llvm::Value *> args;
    for (std::size_t i = 0; i < c.args.size(); ++i) {
      llvm::Value *arg = rvalue(c.args[i]);
      if (i >= fixed && arg->getType()->isFloatTy()) // C passes float as double
        arg = b_.CreateFPExt(arg, b_.getDoubleTy());
      args.push_back(arg);
    }
    return b_.CreateCall(f->getFunctionType(), f, args);
  }

  llvm::Value *cast(const Expr &e, const Cast &c) {
    if (c.kind == CastKind::ArrayToPointer)
      return lvalue(*c.operand);
    if (c.kind == CastKind::NullToPointer)
      return llvm::ConstantPointerNull::get(b_.getPtrTy());
    llvm::Value *v = rvalue(*c.operand);
    llvm::Type *to = lower(e.type);
    switch (c.kind) {
    case CastKind::NoOp:
    case CastKind::PointerCast:
      return v;
    case CastKind::IntegralCast:
      return b_.CreateSExtOrTrunc(v, to);
    case CastKind::IntToFloat:
      return b_.CreateSIToFP(v, to);
    case CastKind::FloatToInt:
      return b_.CreateFPToSI(v, to);
    case CastKind::PointerToInt:
      return b_.CreatePtrToInt(v, to);
    case CastKind::IntToPointer:
      return b_.CreateIntToPtr(b_.CreateSExt(v, b_.getInt64Ty()), to);
    case CastKind::ToVoid:
      return nullptr;
    default:
      assert(false && "unresolved cast reached code generation");
      return nullptr;
    }
  }

  struct Loop {
    llvm::BasicBlock *continueTo;
    llvm::BasicBlock *breakTo;
  };

  llvm::LLVMContext &ctx_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> b_;
  llvm::Function *fn_ = nullptr;
  std::map<SymbolId, llvm::Value *>
      values_; // address of each variable, or the function
  std::map<std::string, llvm::GlobalVariable *> strings_;
  std::vector<Loop> loops_;
};

} // namespace

Result<std::unique_ptr<llvm::Module>>
generateModule(const TranslationUnit &tu, llvm::LLVMContext &context,
               std::string_view moduleName) {
  return ModuleBuilder(context, moduleName).run(tu);
}

} // namespace jia
