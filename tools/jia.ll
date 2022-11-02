%top{
#include <stdio.h>
int summa = 0;
}


DIGIT [0-9]

OP    [\+\-\*\/]

LEFTBRACKET \(

RIGHTBRACKET \)

BLANK [ \t\n\r]



%%

{LEFTBRACKET}  printf("entered (\n");

{RIGHTBRACKET} printf("entered )\n");    


{DIGIT}+	{
			printf("entered a digit: %d\n", atoi(yytext));
			summa += atoi(yytext);
		}


{OP}		{
			printf("entered a operator sign: %s\n", yytext);
		}

{BLANK}

.		printf("Invalid Input, summa = %d", summa);


%%

int main(int argc, char** argv){
	++argv; --argc;
	if(argc > 0){ yyin = fopen(argv[0], "r"); }
	else yyin = stdin;
	yylex();
}
