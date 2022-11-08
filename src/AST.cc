#include "AST.hh"
#include "CodegenVisitor.hh"

llvm::Value* NumberAST::codegen(CodegenVisitor& v){
    return v.retv = llvm::ConstantInt::get(*(v.TheContext), llvm::APInt(32, value));
}

llvm::Value* MainAST::codegen(CodegenVisitor& v){
    
    llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*v.TheContext), false);

    llvm::Function* mainf = llvm::Function::Create(ft,
        llvm::Function::ExternalLinkage, "jialmain", v.TheModule.get());

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*v.TheContext, "main", mainf);

    v.Builder->SetInsertPoint(BB);

    v.Builder->CreateRet(v.retv);

    llvm::verifyFunction(*mainf);

    return mainf;
}