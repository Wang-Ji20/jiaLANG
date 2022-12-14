#pragma once
#include <iostream>
#include <memory>
#include <map>
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

using tnode = std::shared_ptr<AST>;

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
    /* Children */
    std::vector<std::shared_ptr<AST>> children;
    int children_number;
    std::string name;
    std::string Type;
    friend std::ostream & operator<<(std::ostream &out, const AST& ast);

};

#include "CodegenVisitor.hh"

class CompoundStatAST: public AST{
public:
    CompoundStatAST(){
        name = "statement list";
    }
};

class MetaAST: public AST{
public:
    MetaAST(){
        name = "meta program";
    }

};

class FuncDefAST: public AST{
public:
    FuncDefAST(){
        name = "function definition";
    }
    
};


class DeclAST: public AST{
public:
    DeclAST(){
        name = "Decl";
    }    
};

class DirectDclAST: public AST{
public:
    DirectDclAST(){
        name = "DirectDcl";
    }    
};

class ParameterListAST: public AST{
public:
    ParameterListAST(){
        name = "ParameterList";
    }    
};

class ParameterDeclAST: public AST{
public:
    ParameterDeclAST(){
        name = "ParameterDecl";
    }    
};


class InitDeclListAST: public AST{
public:
    InitDeclListAST(){
        name = "InitDeclList";
    }    
};


class InitDeclAST: public AST{
public:
    InitDeclAST(){
        name = "InitDecl";
    }    
};

class InitListAST: public AST{
public:
    InitListAST(){
        name = "InitList";
    }    
};


class LoopStatAST: public AST{
public:
    LoopStatAST(){
        name = "LoopStat";
    }    
};


class IfStatAST: public AST{
public:
    IfStatAST(){
        name = "IfStat";
    }    
};


class IfElseStatAST: public AST{
public:
    IfElseStatAST(){
        name = "IfElseStat";
    }    
};

class ExprStatAST: public AST{
public:
    ExprStatAST(){
        name = "expression statement";
    }
};

class RetStatAST: public AST{
public:
    RetStatAST(){
        name = "return statement";
    }
};


class ContStatAST: public AST{
public:
    ContStatAST(){
        name = "ContStat";
    }    
};


class BreakStatAST: public AST{
public:
    BreakStatAST(){
        name = "BreakStat";
    }    
};


class StatListAST: public AST{
public:
    StatListAST(){
        name = "StatList";
    }    
};


class PrimaryExprAST: public AST{
public:
    PrimaryExprAST(){
        name = "primary expression";
    }
};



class ExprAST: public AST{
public:
    public:
    ExprAST(){
        name = "expression";
    }
};

class ConstAST: public AST{
public:
    ConstAST(){
        name = "const";
    }
    int int_v;
    // llvm::Value* codegen(CodegenVisitor& v) override;
};

class ReadAST: public AST{
public:
    ReadAST(){
        name = "read id from ";
    }
};


class CallExprAST: public AST{
public:
    CallExprAST(){
        name = "CallExpr";
    }    
};



class UnaryExprAST: public AST{
public:
    UnaryExprAST(){
        name = "UnaryExpr";
    }    
};


class BinaryExprAST: public AST{
public:
    BinaryExprAST(){
        name = "BinaryExpr";
    }    
};


class ExprListAST: public AST{
public:
    ExprListAST(){
        name = "ExprList";
    }    
};

class PrintVisitor: public Visitor{
    int layer = 0;
public:
    virtual void visit(std::shared_ptr<AST> ast) override;
};

void
make_function(tnode scope, std::string type_spec, tnode direct_dcl, tnode param_list, tnode func_body);

void
make_decl(tnode scope, std::string type_spec, tnode init_scl_list);

tnode
make_directdcl(std::string identifier);

tnode
make_directdcl_array(tnode direct_dcl, int arrlen = 0);

tnode
make_func_sig(tnode directdcl, tnode param_list = nullptr);

tnode
make_parameter_list(std::string type_specifier, tnode directdcl);

tnode
cons_parameter_list(tnode cdr, std::string car_type, tnode car_dcl);

tnode
make_initdcl_list(tnode init_declarator);

tnode
cons_initdcl_list(tnode cdr, tnode car);

tnode
make_initdcl(tnode direct_dcl, tnode init = nullptr);

tnode
make_init(tnode expr);

tnode
make_init_list(tnode init);

tnode
cons_init_list(tnode cdr, tnode car);

tnode
make_loop(tnode cond, tnode body);

tnode
make_if(tnode cond, tnode body);

tnode
make_ifelse(tnode cond, tnode body, tnode elbody);

tnode
make_expr_stat(tnode expr);

tnode
make_ret_stat(tnode expr = nullptr);

tnode
make_cont();

tnode
make_break();

tnode
make_decl(std::string type, tnode init_dcl_list);

tnode
make_stat_list(tnode stat = nullptr);

tnode
cons_stat_list(tnode cdr, tnode car);

tnode
make_int_c(int i);

tnode
make_float_c(std::string f);

tnode
make_char_c(std::string c);

tnode
read_ident(std::string id);

tnode
func_eval(tnode funcbody, tnode params);

tnode
array_get(tnode id, tnode idx);

tnode
make_unary(std::string op, tnode operand);

tnode
make_binary(std::string op, tnode operand1, tnode operand2);

tnode
make_expr_list(tnode expr);

tnode
cons_expr_list(tnode cdr, tnode car);