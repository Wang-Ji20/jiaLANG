#pragma once
#include "AST.hh"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"


class CodegenVisitor: public Visitor{

/* 因为这个类中含有的都应该是静态变量，因此将它设置为 Singleton(单例) 类，也就是
*   这个类只有一个实例化的对象。
 */

private:
    CodegenVisitor(){
        TheContext = std::make_unique<llvm::LLVMContext>();
        TheModule = std::make_unique<llvm::Module>("test", *TheContext);
        Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
    }
public:
    static CodegenVisitor& GetInstance(){
        static CodegenVisitor ins;
        return ins;
    }
    llvm::Value* llvmError(const char* str){
        fprintf(stderr, "error from llvm: %s", str);
        return nullptr;
    }

/* visit */
    void visit(std::shared_ptr<AST> ast) override;

    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::IRBuilder<>> Builder;
    std::unique_ptr<llvm::Module> TheModule;
    std::map<std::string, llvm::Value *> NamedValues;
    llvm::Value* retv;

/* 生成最终代码 */
    ~CodegenVisitor(){
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
        auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, features, opt, RM);

        TheModule->setDataLayout(TheTargetMachine->createDataLayout());
        auto Filename = "output.out";

        std::error_code EC;
        llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);

        llvm::legacy::PassManager pass;
        auto FileType = llvm::CGFT_ObjectFile;

        if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType))
        {
            llvm::errs() << "TheTargetMachine can't emit a file of this type";
        }

        pass.run(*TheModule);
        dest.flush();

        llvm::outs() << "Wrote " << Filename << "\n";
    }
};