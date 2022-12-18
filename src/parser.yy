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
%token <char> CHAR_CONST
%token <long> INT_CONST
%token <double> FLOAT_CONST

/* 非终结符 */

%nterm <std::shared_ptr<AST>>
    meta
    direct_dcl
    parameter_type_list
    init_declarator_list
    init_declarator
    initializer
    initializer_list
    statement
    compond_statement
    statement_list
    constant
    primary_expression
    unary_expression
    binary_expression
    expression_list
;

%nterm <std::string> type_specifier 

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
    type_specifier direct_dcl LPAREN RPAREN compond_statement         { make_function(drv.root, $1, $2, nullptr, $5); }
|   type_specifier direct_dcl LPAREN parameter_type_list RPAREN compond_statement { make_function(drv.root, $1, $2, $4, $6); }
|   type_specifier init_declarator_list  ";"            { make_decl(drv.root, $1, $2); }
|   meta type_specifier direct_dcl LPAREN RPAREN compond_statement    { make_function(drv.root, $2, $3, nullptr, $6); }
|   meta type_specifier direct_dcl LPAREN parameter_type_list RPAREN compond_statement    { make_function(drv.root, $2, $3, $5, $7); }
|   meta type_specifier init_declarator_list ";"        { make_decl(drv.root, $2, $3); }
;

type_specifier:
    VOID    {$$ = "void";}
|   CHAR    {$$ = "char";}
|   INT     {$$ = "int";}
|   FLOAT   {$$ = "float";}
;


direct_dcl:
    IDENTIFIER                                          { $$ = make_directdcl($1); }
|   LPAREN direct_dcl RPAREN                            { $$ = $2; }
|   direct_dcl LBRACK RBRACK                            { $$ = make_directdcl_array($1); }
|   direct_dcl LBRACK INT_CONST RBRACK                  { $$ = make_directdcl_array($1, $3); }
;

parameter_type_list:
    type_specifier direct_dcl                           { $$ = make_parameter_list($1, $2); }
|   parameter_type_list COMMA type_specifier direct_dcl { $$ = cons_parameter_list($1, $3, $4); }
;

init_declarator_list:
    init_declarator                                     { $$ = make_initdcl_list($1); }
|   init_declarator_list COMMA init_declarator          { $$ = cons_initdcl_list($1, $3); }
;

init_declarator:
    direct_dcl                                          { $$ = make_initdcl($1); }
|   direct_dcl "=" initializer                          { $$ = make_initdcl($1, $3); }
;

initializer:
    binary_expression                                   { $$ = make_init($1); }
|   LBRACE initializer_list RBRACE                      { $$ = make_init($2); }
|   LBRACE initializer_list COMMA RBRACE                { $$ = make_init($2); }
;

initializer_list:
    initializer                                         { $$ = make_init_list($1); }
|   initializer_list COMMA initializer                  { $$ = cons_init_list($1, $3); }
;

statement:
    WHILE LPAREN binary_expression RPAREN compond_statement { $$ = make_loop($3, $5); }
|   IF LPAREN binary_expression RPAREN compond_statement    { $$ = make_if($3, $5); }
|   IF LPAREN binary_expression RPAREN compond_statement ELSE compond_statement { $$ = make_ifelse($3, $5, $7); }
|   ";" { ; }
|   binary_expression ";"   { $$ = make_expr_stat($1); }
|   RET binary_expression ";"   { $$ = make_ret_stat($2); }
|   RET ";" { $$ = make_ret_stat(); }
|   CONTINUE ";" { $$ = make_cont(); }
|   BREAK ";"   { $$ = make_break(); }
|   compond_statement   { $$ = $1;}
;

compond_statement:
    LBRACE RBRACE   { $$ = make_stat_list();}
|   LBRACE statement_list RBRACE { $$ = $2; }
;

statement_list:
    statement   { $$ = make_stat_list($1); }
|   type_specifier init_declarator_list ";" {$$ = make_stat_list(); make_decl($$, $1, $2);}
|   statement_list type_specifier init_declarator_list ";" {make_decl($1, $2, $3); $$ = $1;}
|   statement_list statement { $$ = cons_stat_list($1, $2); }
;


constant:
    INT_CONST   {$$ = make_int_c($1);}
|   FLOAT_CONST {$$ = make_float_c($1);}
|   CHAR_CONST  {$$ = make_char_c($1);}
;


primary_expression:
    constant    { $$ = $1;}
|   IDENTIFIER  { $$ = read_ident($1); }
|   LPAREN binary_expression RPAREN { $$ = $2; }
|   primary_expression LPAREN expression_list RPAREN { $$ = func_eval($1, $3); }
|   primary_expression LBRACK binary_expression RBRACK { $$ = array_get($1, $3); }
;


unary_expression:
    primary_expression  { $$ = $1; }
|   LPAREN type_specifier RPAREN unary_expression { $$ = make_unary($2, $4);}
|   MINUS unary_expression { $$ = make_unary("-", $2);}
|   LOGICAL_NEG unary_expression { $$ = make_unary("!", $2);}
|   STAR unary_expression { $$ = make_unary("*", $2);}
|   ADDR unary_expression { $$ = make_unary("&", $2);}
;


binary_expression:
    unary_expression { $$ = $1; }
|   binary_expression STAR unary_expression {$$ = make_binary("*", $1, $3); }
|   binary_expression DIVIDE unary_expression {$$ = make_binary("/", $1, $3); }
|   binary_expression MODUS unary_expression {$$ = make_binary("%", $1, $3); }
|   binary_expression ADD unary_expression {$$ = make_binary("+", $1, $3); }
|   binary_expression MINUS unary_expression {$$ = make_binary("-", $1, $3); }
|   binary_expression GT unary_expression {$$ = make_binary(">", $1, $3); }
|   binary_expression LS unary_expression {$$ = make_binary("<", $1, $3); }
|   binary_expression GTE unary_expression {$$ = make_binary(">=", $1, $3); }
|   binary_expression LSE unary_expression {$$ = make_binary("<=", $1, $3); }
|   binary_expression EQ unary_expression {$$ = make_binary("==", $1, $3); }
|   binary_expression NEQ unary_expression {$$ = make_binary("!=", $1, $3); }
|   binary_expression AND unary_expression {$$ = make_binary("&&", $1, $3); }
|   binary_expression OR unary_expression {$$ = make_binary("||", $1, $3); }
|   unary_expression ASSIGN binary_expression {$$ = make_binary("=", $1, $3); }
;

expression_list:
    binary_expression   { $$ = make_expr_list($1); }
|   expression_list COMMA binary_expression { $$ = cons_expr_list($1, $3); }
;

%%

/* 输出错误的函数 */

void
yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}

std::ostream & operator<<(std::ostream &out, const AST& ast){
    out << ast.name << std::endl;
    return out;
}