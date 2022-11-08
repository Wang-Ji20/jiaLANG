/* 指定语言为 C++ */
%skeleton "lalr1.cc"

/* 约定 bison 版本为3.5.1 即 ubuntu 20.04LTS 自带版本 */
%require "3.5.1"

/* 要求输出一个头文件，其中包含所有 Token 的值。这个头文件给 flex 用。 */
%defines

/* 设置 token 是 variant（可变类型）, 为 token 提供多种类型支持。*/
%define api.token.constructor
%define api.value.type variant
%define parse.assert

/* 引入 driver 和 string 类型 */

%code requires{
    #include <string>
    #include "AST.hh"
    class driver;
}

/* 这条指令有两个作用：1. 给 flex 一个 driver 2. 给 parser 一个 driver*/
%param {driver& drv}

/* 启用位置追踪。解析的时候会记录 token 的位置 */
%locations

/* 启用解析器追踪，可以输出语法解析的过程 */
%define parse.trace
%define parse.error verbose
%define parse.lac full

/* 把 driver 拿进来 */
%code {
    #include "driver.hh"
}

/* 接下来定义 Token */

/* 给 Token 加上前缀 */
%define api.token.prefix {TOK_}

/* 三类 token */

/* 关键字 */
%token
    END 0 "end of file"
    MAIN "main"
    INT "int"
    RET "return"
    LPAREN "("
    RPAREN ")"
    LBRACE "{"
    RBRACE "}"
    SEMICOLON ";"
; 
%token <std::string> IDENTIFIER
%token <int> NUMBER

/* 非终结符 */
%nterm <StatAST> statement
%nterm <PrimaryExprAST> primary_expression
%nterm <JPStatAST> jump_statement
%nterm <ExprStatAST> expression_statement
%nterm <ExprAST> expression
%nterm <MainAST> meta

/* Debug Tracing 里面如何打印一个值？ 直接用流运算符打印即可。 */
%printer { yyo << $$; } <*>;

%%
%start meta;

meta: 
    INT MAIN statement 
    {
        $$.LCT = std::make_shared<StatAST>(std::move($3));
        drv.root = std::make_shared<MainAST>(std::move($$));
    }
;

expression:
    primary_expression
    {
        $$.LCT = std::make_shared<PrimaryExprAST>(std::move($1));
        $$.value = $$.LCT->value;
    }
;

primary_expression:
    NUMBER          
    {
        $$.LCT = std::make_shared<NumberAST>(std::move($1));
        $$.value = $$.LCT->value;
    }
;

statement:
    expression_statement
    {
        $$.LCT = std::make_shared<ExprStatAST>(std::move($1));
    }
|    jump_statement
    {
        $$.LCT = std::make_shared<JPStatAST>(std::move($1));
        $$.value = $$.LCT->value;
    }
;

expression_statement:
    ";"
    {
        $$.LCT = std::make_shared<ExprAST>();
    }
|    expression ";"
    {
        $$.LCT = std::make_shared<ExprAST>(std::move($1));
    }
;

jump_statement:
    RET expression ";"
    {
        $$.LCT = std::make_shared<ExprAST>(std::move($2));
        $$.value = $$.LCT->value;
    }
;

%%

/* 输出错误的函数 */

void
yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}

std::ostream & operator<<(std::ostream &out, const AST& ast){
    out << "type is " << ast.name << " value is " << ast.value;
    return out;
}