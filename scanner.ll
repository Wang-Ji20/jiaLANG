%{ /* -*- C++ -*- */
# include <iostream>
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <cstring> // strerror
# include <string>
%}

%option noyywrap nounput noinput batch

%%
break |
char |
const |
continue |
else |
float |
if |
int |
return |
sizeof |
void |
while { printf("<KEYWORD, %s>\n", yytext); }

[a-zA-Z_][a-zA-Z_0-9]* { printf("<IDENTIFIER, %s>\n", yytext); }

"+" |
"-" |
"*" |
"/" |
"%" |
"!" |
"=" |
"&" |
"," |
";" |
"{" |
"}" |
"[" |
"]" |
"(" |
")" |    

">"  |
"<"  |
">=" |
"<=" |
"==" |
"!=" |
"&&" |
"||"    { printf("<SYMBOL, %s>\n", yytext); }

0|[1-9][0-9]* { printf("<CONSTANT_I, %s>\n", yytext); }
(0|[1-9][0-9]*)("."[0-9]+)?([Ee][+-]?(0|[1-9][0-9]*))? { printf("<CONSTANT_F, %s>\n", yytext); }
"'"([^'\\\n]|(\\(['"\?\\abfnrtv]|[0-7]{1,3}|x[a-fA-F0-9]+)))+"'" { printf("<CONSTANT_C, %s>\n", yytext); }
\"([^"\\\n]|(\\(['"\?\\abfnrtv]|[0-7]{1,3}|x[a-fA-F0-9]+)))*\" { printf("<CONSTANT_S, %s>\n", yytext); }

"//".*
[ \t\v\n\f\r]
. { printf("Unexpected character!\n"); }

<<EOF>> { return 0; }
%%

void scan_begin (std::string file)
{
  if (file.empty () || file == "-")
    /* yyin 代表 scanner 的来源 */
    yyin = stdin;
  else if (!(yyin = fopen (file.c_str (), "r")))
      {
      std::cerr << "cannot open " << file << ": " << strerror(errno) << '\n';
      exit (EXIT_FAILURE);
    }
}

void scan_end ()
  {
  fclose (yyin);
}

int main (int argc, char *argv[])
{
  scan_begin(argv[1]);
  printf("%d", yylex());
  return 0;
}
