%{ /* -*- C++ -*- */
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <cstring> // strerror
# include <string>
# include "driver.hh"
# include "parser.hh"
%}


%option noyywrap nounput noinput batch debug

%{
    yy::parser::symbol_type
    make_INT_CONST (const std::string &s, const yy::parser::location_type& loc);
    yy::parser::symbol_type
    make_FLOAT_CONST (const std::string &s, const yy::parser::location_type& loc);
    yy::parser::symbol_type
    make_CHAR_CONST (const std::string &s, const yy::parser::location_type& loc);
%}

id    [a-zA-Z][a-zA-Z_0-9]*
int   [0-9]+
float (0|[1-9][0-9]*)("."[0-9]+)?([Ee][+-]?(0|[1-9][0-9]*))?
char  "'"([^'\\\n]|(\\(['"\?\\abfnrtv]|[0-7]{1,3}|x[a-fA-F0-9]+)))+"'"
blank [ \t\v\f\r]

%{
    #define YY_USER_ACTION loc.columns(yyleng);
%}
%%
%{
  // A handy shortcut to the location held by the driver.
  yy::location& loc = drv.location;
  // Code run each time yylex is called.
  // step 将位置的 begin 移到 end，相当于前进了一步。

  /* 如果 yyleng 非空，则前进 yyleng 行，重置 column。yyleng 是当前 token 的长度。 */
  loc.step ();
%}
{blank}+   loc.step ();

\n+        loc.lines (yyleng); loc.step ();

"break"     return yy::parser::make_BREAK(loc);
"char"      return yy::parser::make_CHAR(loc);
"continue"  return yy::parser::make_CONTINUE(loc);
"else"      return yy::parser::make_ELSE(loc);
"float"     return yy::parser::make_FLOAT(loc);
"if"        return yy::parser::make_IF(loc);
"int"       return yy::parser::make_INT  (loc);
"return"    return yy::parser::make_RET   (loc);
"void"      return yy::parser::make_VOID(loc);
"while"     return yy::parser::make_WHILE(loc);

"+"         return yy::parser::make_ADD(loc);
"-"         return yy::parser::make_MINUS(loc);
"*"         return yy::parser::make_STAR(loc);
"/"         return yy::parser::make_DIVIDE(loc);
"%"         return yy::parser::make_MODUS(loc);
"!"         return yy::parser::make_LOGICAL_NEG(loc);
"="         return yy::parser::make_ASSIGN(loc);
"&"         return yy::parser::make_ADDR(loc);
","         return yy::parser::make_COMMA(loc);
";"         return yy::parser::make_SEMICOLON  (loc);
"("         return yy::parser::make_LPAREN (loc);
")"         return yy::parser::make_RPAREN (loc);
"["         return yy::parser::make_LBRACK(loc);
"]"         return yy::parser::make_RBRACK(loc);
"{"         return yy::parser::make_LBRACE (loc);
"}"         return yy::parser::make_RBRACE (loc);
">"         return yy::parser::make_GT(loc);
"<"         return yy::parser::make_LS(loc);
">="        return yy::parser::make_GTE(loc);
"<="        return yy::parser::make_LSE(loc);
"=="        return yy::parser::make_EQ(loc);
"!="        return yy::parser::make_NEQ(loc);
"&&"        return yy::parser::make_AND(loc);
"||"        return yy::parser::make_OR(loc);

{int}       return make_INT_CONST (yytext, loc);
{id}        return yy::parser::make_IDENTIFIER (yytext, loc);
{float}     return make_FLOAT_CONST(yytext, loc);
{char}      return make_CHAR_CONST(yytext, loc);


.          {
             yy::parser::syntax_error
               (loc, "invalid character: " + std::string(yytext));
            yy::parser::make_END (loc);
}
<<EOF>>    return yy::parser::make_END (loc);
%%

yy::parser::symbol_type
make_INT_CONST (const std::string &s, const yy::parser::location_type& loc)
{
  errno = 0;
  long n = strtol (s.c_str(), NULL, 10);
  if (errno == ERANGE)
{    yy::parser::syntax_error (loc, "integer is out of range: " + s);
    return yy::parser::make_END (loc);}
  return yy::parser::make_INT_CONST (n, loc);
}

yy::parser::symbol_type
make_FLOAT_CONST(const std::string &s, const yy::parser::location_type& loc)
{
  errno = 0;
  double d = strtod(s.c_str(), nullptr);
  if(errno == ERANGE) {
    yy::parser::syntax_error(loc, "double is out of range: " + s);
    return yy::parser::make_END(loc);
  }
  return yy::parser::make_FLOAT_CONST(d, loc);
}

yy::parser::symbol_type
make_CHAR_CONST(const std::string &s, const yy::parser::location_type& loc)
{
  char c;
  if(s[1] != '\\'){
    c = s[1];
  }
  else{
    switch(s[2]){
      case 'n':
        c = '\n';
        break;
      case '\\':
        c = '\\';
        break;
      case '0':
        c = '\0';
        break;
      case '\r':
        c = '\r';
        break;
      default:
        break;
    }
  }
  return yy::parser::make_FLOAT_CONST(c, loc);
}

void
driver::scan_begin ()
{
  /* yy_flex_debug 表示 scanner 是否为 debug 模式 */
  yy_flex_debug = trace_scanning;
  if (file.empty () || file == "-")
  /* yyin 代表 scanner 的来源 */
    yyin = stdin;
  else if (!(yyin = fopen (file.c_str (), "r")))
    {
      std::cerr << "cannot open " << file << ": " << strerror(errno) << '\n';
      exit (EXIT_FAILURE);
    }
}

void
driver::scan_end ()
{
  fclose (yyin);
}
