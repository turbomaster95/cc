%option noyywrap

D                       [0-9]
L                       [a-zA-Z_]
H                       [a-fA-F0-9]
E                       ([Ee][+-]?{D}+)
P                       ([Pp][+-]?{D}+)
FS                      (f|F|l|L)
IS                      ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "tab.h"
#include <nus.h>

void comment(void);
void count(void);
 int check_type(void);
void add_type(const char *name);
void error(const char *s);
void complain(void);

%}

%%
"/*"               { comment(); }
"//"[^\n]*         { /* consume //-comment */ }
^[ \t]*#.*[\r\n]   { /* ignore preprocessor directives */ }
		   
		   
"auto"             { count(); return(AUTO); }
"_Bool"            { count(); return(BOOL); }
"break"            { count(); return(BREAK); }
"case"             { count(); return(CASE); }
"char"             { count(); return(CHAR); }
"_Complex"         { count(); return(COMPLEX); }
"const"            { count(); return(CONST); }
"continue"         { count(); return(CONTINUE); }
"default"          { count(); return(DEFAULT); }
"do"               { count(); return(DO); }
"double"           { count(); return(DOUBLE); }
"else"             { count(); return(ELSE); }
"enum"             { count(); return(ENUM); }
"extern"           { count(); return(EXTERN); }
"float"            { count(); return(FLOAT); }
"for"              { count(); return(FOR); }
"goto"             { count(); return(GOTO); }
"if"               { count(); return(IF); }
"_Imaginary"       { count(); return(IMAGINARY); }
"inline"           { count(); return(INLINE); }
"asm"              { count(); return(ASM); }
"__asm__"          { count(); return(ASM); }
"int"              { count(); return(INT); }
"long"             { count(); return(LONG); }
"register"         { count(); return(REGISTER); }
"restrict"         { count(); return(RESTRICT); }
"return"           { count(); return(RETURN); }
"short"            { count(); return(SHORT); }
"signed"           { count(); return(SIGNED); }
"sizeof"           { count(); return(SIZEOF); }
"static"           { count(); return(STATIC); }
"struct"           { count(); return(STRUCT); }
"switch"           { count(); return(SWITCH); }
"typedef"          { count(); return(TYPEDEF); }
"union"            { count(); return(UNION); }
"unsigned"         { count(); return(UNSIGNED); }
"void"             { count(); return(VOID); }
"volatile"         { count(); return(VOLATILE); }
"while"            { count(); return(WHILE); }

{L}({L}|{D})*        { 
    count(); 
    int token = check_type();
    // Copy the text of the identifier over to Bison
    yylval.str_val = nu_strdup(yytext);
    return token;
}

0[xX]{H}+{IS}?        { 
    count(); 
    yylval.int_val = strtoll(yytext, NULL, 16); 
    return(CONSTANT); 
}
0[0-7]*{IS}?        { 
    count(); 
    yylval.int_val = strtoll(yytext, NULL, 8); 
    return(CONSTANT); 
}
[1-9]{D}*{IS}?        { 
    count(); 
    yylval.int_val = strtoll(yytext, NULL, 10); 
    return(CONSTANT); 
}
L?'(\\.|[^\\'\n])+'    { 
    count(); 
    if (yytext[0] == 'L') {
        yylval.int_val = yytext[2];
    } else {
        yylval.int_val = yytext[1];
    }
    return(CONSTANT); 
}

{D}+{E}{FS}?                { count(); yylval.int_val = strtoll(yytext, NULL, 10); return(CONSTANT); }
{D}*"."{D}+{E}?{FS}?        { count(); yylval.int_val = (int64_t)strtod(yytext, NULL); return(CONSTANT); }
{D}+"."{D}*{E}?{FS}?        { count(); yylval.int_val = (int64_t)strtod(yytext, NULL); return(CONSTANT); }
0[xX]{H}+{P}{FS}?           { count(); yylval.int_val = strtoll(yytext, NULL, 16); return(CONSTANT); }
0[xX]{H}*"."{H}+{P}?{FS}?   { count(); yylval.int_val = strtoll(yytext, NULL, 16); return(CONSTANT); }
0[xX]{H}+"."{H}*{P}?{FS}?   { count(); yylval.int_val = strtoll(yytext, NULL, 16); return(CONSTANT); }


L?\"(\\.|[^\\"\n])*\"    { 
    count(); 
    yylval.str_val = nu_strdup(yytext);
    return(STRING_LITERAL); 
}

"..."            { count(); return(ELLIPSIS); }
">>="            { count(); return(RIGHT_ASSIGN); }
"<<="            { count(); return(LEFT_ASSIGN); }
"+="             { count(); return(ADD_ASSIGN); }
"-="             { count(); return(SUB_ASSIGN); }
"*="             { count(); return(MUL_ASSIGN); }
"/="             { count(); return(DIV_ASSIGN); }
"%="             { count(); return(MOD_ASSIGN); }
"&="             { count(); return(AND_ASSIGN); }
"^="             { count(); return(XOR_ASSIGN); }
"|="             { count(); return(OR_ASSIGN); }
">>"             { count(); return(RIGHT_OP); }
"<<"             { count(); return(LEFT_OP); }
"++"             { count(); return(INC_OP); }
"--"             { count(); return(DEC_OP); }
"->"             { count(); return(PTR_OP); }
"&&"             { count(); return(AND_OP); }
"||"             { count(); return(OR_OP); }
"<="             { count(); return(LE_OP); }
">="             { count(); return(GE_OP); }
"=="             { count(); return(EQ_OP); }
"!="             { count(); return(NE_OP); }
";"              { count(); return(';'); }
("{"|"<%")       { count(); return('{'); }
("}"|"%>")       { count(); return('}'); }
","              { count(); return(','); }
":"              { count(); return(':'); }
"="              { count(); return('='); }
"("              { count(); return('('); }
")"              { count(); return(')'); }
("["|"<:")       { count(); return('['); }
("]"|":>")       { count(); return(']'); }
"."              { count(); return('.'); }
"&"              { count(); return('&'); }
"!"              { count(); return('!'); }
"~"              { count(); return('~'); }
"-"              { count(); return('-'); }
"+"              { count(); return('+'); }
"*"              { count(); return('*'); }
"/"              { count(); return('/'); }
"%"              { count(); return('%'); }
"<"              { count(); return('<'); }
">"              { count(); return('>'); }
"^"              { count(); return('^'); }
"|"              { count(); return('|'); }
"?"              { count(); return('?'); }

[ \t\v\n\f]      { count(); }
.                { count(); complain(); }

%%

void comment(void)
{
    char c, prev = 0;
  
    while ((c = input()) != 0)      /* (EOF maps to 0) */
    {
        if (c == '/' && prev == '*')
            return;
        prev = c;
    }
    error("unterminated comment");
}


int column = 0;

void count(void)
{
    int i;

    for (i = 0; yytext[i] != '\0'; i++)
        if (yytext[i] == '\n')
            column = 0;
        else if (yytext[i] == '\t')
            column += 8 - (column % 8);
        else
            column++;

   // ECHO;
}


int check_type(void)
{
    if (is_registered_type(yytext)) {
        return TYPE_NAME; 
    }
    return IDENTIFIER;
}

void error(const char *s)
{
    fprintf(stderr, "Lexical Error: %s\n", s);
}

void complain(void)
{
    printf("W: Unmatched Characters!");
}
