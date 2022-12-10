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
    void emit_tree(int lay, std::string word){
        std::cout << word;
        for(int i = 0; i < lay; i++)
            std::cout << '.';
        std::cout << std::endl;
    }

}

/* 接下来定义 Token */

/* 给 Token 加上前缀 */
%define api.token.prefix {TOK_}

/* 三类 token */

/* 关键字 */
%token
    BREAK "break"
    CHAR "char"
    CONTINUE "continue"
    ELSE "else"
    FLOAT "float"
    IF "if"
    INT "int"
    RET "return"
    VOID "void"
    WHILE "while"

    ADD "+"
    MINUS "-"
    STAR "*"
    DIVIDE "/"
    MODUS "%"
    LOGICAL_NEG "!"
    ASSIGN "="
    ADDR "&"
    COMMA ","
    SEMICOLON ";"
    LPAREN "("
    RPAREN ")"
    LBRACK "["
    RBRACK "]"
    LBRACE "{"
    RBRACE "}"

    GT ">"
    LS "<" 
    GTE ">="
    LSE "<="
    EQ "=="
    NEQ "!="
    AND "&&"
    OR "||"

    END 0 "end of file"
; 

%token <std::string> IDENTIFIER
%token <std::string> STRING_CONST
%token <std::string> CHAR_CONST
%token <int> INT_CONST
%token <std::string> FLOAT_CONST

/* 非终结符 */

/* Debug Tracing 里面如何打印一个值？ 直接用流运算符打印即可。 */
%printer { yyo << $$; } <*>;

%left ','
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NEQ
%left '<' '>' ">=" "<="
%left '+' '-'
%left '*' '/' '%'

%%
%start meta;

meta: 
    external_declaration
|   meta external_declaration
;

external_declaration:
    function_definition
|   declaration
;

function_definition:
    type_specifier direct_dcl compond_statement
;

type_specifier:
    VOID
|   CHAR
|   INT
|   FLOAT
;


direct_dcl:
    IDENTIFIER
|   LPAREN direct_dcl RPAREN
|   direct_dcl LBRACK RBRACK
|   direct_dcl LBRACK binary_expression RBRACK
|   direct_dcl LPAREN RPAREN
|   direct_dcl LPAREN parameter_type_list RPAREN
;

parameter_type_list:
    parameter_declaration
|   parameter_type_list COMMA parameter_declaration
;

parameter_declaration:
    type_specifier direct_dcl
;


direct_adcl:
    LBRACK binary_expression RBRACK
|   direct_adcl LBRACK binary_expression RBRACK
|   direct_adcl LPAREN RPAREN
|   LPAREN RPAREN
|   direct_adcl LPAREN parameter_type_list RPAREN
|   LPAREN parameter_type_list RPAREN
;

declaration:
    type_specifier init_declarator_list
|   type_specifier
;

init_declarator_list:
    init_declarator
|   init_declarator_list COMMA init_declarator
;

init_declarator:
    direct_dcl
|   direct_dcl "=" initializer
;

initializer:
    binary_expression
|   LBRACE initializer_list RBRACE
|   LBRACE initializer_list COMMA RBRACE
;

initializer_list:
    initializer_list COMMA initializer
|   initializer
;

statement:
    WHILE LPAREN binary_expression RPAREN compond_statement
|   IF LPAREN binary_expression RPAREN compond_statement
|   IF LPAREN binary_expression RPAREN compond_statement ELSE compond_statement
|   ";"
|   binary_expression ";"
|   RET binary_expression ";"
|   RET ";"
|   CONTINUE ";"
|   BREAK ";"
|   compond_statement
;

compond_statement:
    LBRACE RBRACE
|   LBRACE declaration_list RBRACE
|   LBRACE statement_list RBRACE
|   LBRACE declaration_list statement_list RBRACE
;

declaration_list:
    declaration
|   declaration_list declaration
;

statement_list:
    statement
|   statement_list statement
;


constant:
    INT_CONST
|   FLOAT_CONST
|   CHAR_CONST
|   STRING_CONST
;


primary_expression:
    constant
|   IDENTIFIER
|   LPAREN binary_expression RPAREN
|   primary_expression LPAREN expression_list RPAREN
|   primary_expression LBRACK binary_expression RBRACK
;


unary_expression:
    primary_expression
|   LPAREN type_specifier RPAREN unary_expression
|   ADD unary_expression
|   MINUS unary_expression
|   LOGICAL_NEG unary_expression
|   STAR unary_expression
|   ADDR unary_expression
;


binary_expression:
    unary_expression
|   binary_expression STAR unary_expression
|   binary_expression DIVIDE unary_expression
|   binary_expression MODUS unary_expression
|   binary_expression ADD unary_expression
|   binary_expression MINUS unary_expression
|   binary_expression ">" unary_expression
|   binary_expression "<" unary_expression
|   binary_expression ">=" unary_expression
|   binary_expression "<=" unary_expression
|   binary_expression EQ unary_expression
|   binary_expression NEQ unary_expression
|   binary_expression AND unary_expression
|   binary_expression OR unary_expression
|   unary_expression ASSIGN binary_expression
;

expression_list:
    binary_expression
|   expression_list COMMA binary_expression
;

%%

/* 输出错误的函数 */

void
yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}
