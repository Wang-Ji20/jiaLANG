#pragma once
#include "AST.hh"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

class CodegenVisitor : public Visitor {

  /* 因为这个类中含有的都应该是静态变量，因此将它设置为 Singleton(单例)
   * 类，也就是 这个类只有一个实例化的对象。
   */

private:
  CodegenVisitor() {
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>("test", *TheContext);
    Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
    SymbolTables.push_back(rootSymbolTable);
    // TypeTables.push_back(rootTypeTable);
  }

public:
  static CodegenVisitor &GetInstance() {
    static CodegenVisitor ins;
    return ins;
  }
  llvm::Value *llvmError(const char *str) {
    fprintf(stderr, "error from llvm: %s", str);
    return nullptr;
  }

  llvm::Type *s2Type(std::string s) {
    if (s == "void") {
      return llvm::Type::getVoidTy(*TheContext);
    } else if (s == "int") {
      return llvm::Type::getInt64Ty(*TheContext);
    } else if (s == "float") {
      return llvm::Type::getDoubleTy(*TheContext);
    } else if (s == "char") {
      return llvm::Type::getInt8Ty(*TheContext);
    }
    return llvm::Type::getVoidTy(*TheContext);
  }

  llvm::Value *getSymbolValue(std::string name) {
    for (auto st = SymbolTables.rbegin(); st != SymbolTables.rend(); st++) {
      if ((*st).find(name) != (*st).end()) {
        return (*st)[name];
      }
    }
    return nullptr;
  }

  void outSymbolTable() {
    printf("-------- Symbol Tables --------\n");
    for (auto &&st : SymbolTables) {
      printf(" --- table ---\n");
      for (auto &&ste : st) {
        std::string print_t, print_v;
        llvm::raw_string_ostream rto(print_t), rvo(print_v);
        ste.second->getType()->print(rto);
        ste.second->print(rvo);
        std::cout << ste.first << ' ' << print_v << " type " << print_t << '\n';
      }
      printf(" --- table end --- \n");
    }
  }

  /* visit */
  void visit(std::shared_ptr<AST> ast) override;

  std::string vstname = "codegen";
  std::unique_ptr<llvm::LLVMContext> TheContext;
  // The context 是一个很抽象的东西
  // 它无处不在 又没有什么意义
  // 这个东西的作用似乎是管理静态变量
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  // 这个是最有用的 它可以用来新建语句 写入语句
  std::unique_ptr<llvm::Module> TheModule;
  // The module 具体一点
  // 你还记得我们刚刚看的文件有个 Module 名吗？1
  // 每个文件都是一个 module
  std::map<std::string, llvm::Value *> rootSymbolTable;
  // 这个是根符号表 里面存的是各种 (变量名, 变量地址) 对
  // std::map<std::string, llvm::Type *> rootTypeTable;
  std::vector<std::map<std::string, llvm::Value *>> SymbolTables;
  // 这个是符号表栈
  // 不止一个符号表 局部变量的符号表
  // std::vector<std::map<std::string, llvm::Type *>> TypeTables;
  std::vector<llvm::Value *> retValues;
  // 返回值栈 函数套函数不止一个返回值
  // llvm::Value 类 代表 llvm ir 中的一个值 就是 %1 %2 ...
  // llvm::Type 类 代表 llvm ir 中的值的类型 就是 i64 i64* ... 我已经完全掌握了
  // 下面转到 codegenvisitor.cc
  std::vector<llvm::BasicBlock *> loopHeads, loopEnds;

  void irOpt() {
    /* 中间代码优化 */
    // Create the analysis managers.
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    // Create the new pass manager builder.
    // Take a look at the PassBuilder constructor parameters for more
    // customization, e.g. specifying a TargetMachine or various debugging
    // options.
    llvm::PassBuilder PB;

    // Register all the basic analyses with the managers.
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Create the pass manager.
    // This one corresponds to a typical -O2 optimization pipeline.
    llvm::ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);

    // Optimize the IR!
    MPM.run(*TheModule, MAM);
  }

  /* 生成最终代码 */
  ~CodegenVisitor() {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    TheModule->setTargetTriple(TargetTriple);

    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
    if (!Target) {
      llvm::errs() << Error;
    }

    auto CPU = "generic";
    auto features = "";
    llvm::TargetOptions opt;

    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto TheTargetMachine =
        Target->createTargetMachine(TargetTriple, CPU, features, opt, RM);

    TheModule->setDataLayout(TheTargetMachine->createDataLayout());
    auto Filename = "output.out";

    std::error_code EC;
    llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);

    llvm::legacy::PassManager pass;
    auto FileType = llvm::CGFT_ObjectFile;

    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
      llvm::errs() << "TheTargetMachine can't emit a file of this type";
    }

    pass.run(*TheModule);
    dest.flush();

    llvm::outs() << "Wrote " << Filename << "\n";
  }
};