#pragma once
#include <iostream>
#include <memory>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

/* AST 抽象语法树，我们代码生成的数据结构 */
class AST;
class ExprAST;
class NumberExprAST;
class MainAST;

/* Visitor 访问者，我们代码生成的算法。 */
class Visitor;
class CodegenVisitor;
/*  访问者 visitor 设计模式，解耦解析与代码生成两个步骤。
*/

class Visitor{
    public:
    virtual void visit(std::shared_ptr<AST> ast) = 0;
};

class AST: public std::enable_shared_from_this<AST>{
    public:

    virtual void accept(Visitor* v){
        v->visit(shared_from_this());
    }

    virtual llvm::Value* codegen(CodegenVisitor& v){
        return nullptr;
    }
    /* Left Child Tree & Right Child Tree */
    std::shared_ptr<AST> LCT;
    std::shared_ptr<AST> RCT;
    std::string name;
    int value;

    friend std::ostream & operator<<(std::ostream &out, const AST& ast);

};

#include "CodegenVisitor.hh"

class StatAST: public AST{
public:
    StatAST(){
        name = "statement";
    }
};

class PrimaryExprAST: public AST{
public:
    PrimaryExprAST(){
        name = "primary expression";
    }
};

class JPStatAST: public StatAST{
public:
    JPStatAST(){
        name = "jump statement";
    }
};

class ExprStatAST: public StatAST{
public:
    ExprStatAST(){
        name = "expression statement";
    }
};

class ExprAST: public AST{
    public:
    ExprAST(){
        name = "expression";
    }
};

class NumberAST: public AST{
    
public:
    NumberAST(int _value){
        value = _value;
        name = "number";
    }
    llvm::Value* codegen(CodegenVisitor& v) override;
};

class MainAST: public AST{
    
public:
    MainAST(){
        name = "main";
    }
    llvm::Value* codegen(CodegenVisitor& v) override;
};

class PrintVisitor: public Visitor{
public:
    virtual void visit(std::shared_ptr<AST> ast) override;
};
