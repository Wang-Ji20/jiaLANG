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
    make_NUMBER (const std::string &s, const yy::parser::location_type& loc);
%}

id    [a-zA-Z][a-zA-Z_0-9]*
int   [0-9]+
blank [ \t\r]

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

"int"        return yy::parser::make_INT  (loc);
"main"        return yy::parser::make_MAIN   (loc);
"return"        return yy::parser::make_RET   (loc);
";"        return yy::parser::make_SEMICOLON  (loc);
"("        return yy::parser::make_LPAREN (loc);
")"        return yy::parser::make_RPAREN (loc);
"{"       return yy::parser::make_LBRACE (loc);
"}"       return yy::parser::make_RBRACE (loc);

{int}      return make_NUMBER (yytext, loc);
{id}       return yy::parser::make_IDENTIFIER (yytext, loc);
.          {
             yy::parser::syntax_error
               (loc, "invalid character: " + std::string(yytext));
            yy::parser::make_END (loc);
}
<<EOF>>    return yy::parser::make_END (loc);
%%

yy::parser::symbol_type
make_NUMBER (const std::string &s, const yy::parser::location_type& loc)
{
  errno = 0;
  long n = strtol (s.c_str(), NULL, 10);
  if (! (INT_MIN <= n && n <= INT_MAX && errno != ERANGE))
{    yy::parser::syntax_error (loc, "integer is out of range: " + s);
    return yy::parser::make_END (loc);}
  return yy::parser::make_NUMBER ((int) n, loc);
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
