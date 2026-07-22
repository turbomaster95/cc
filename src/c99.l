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

extern char *current_filename;
extern int errlimt;

int tok_line = 1;
int tok_col  = 1;

int line_no = 1;
int column = 1;

#define TOKEN_START() do { tok_line = line_no; tok_col = column; } while(0)

void comment(void);
void count(void);
 int check_type(void);
void add_type(const char *name);
void cerror(const char *s);
void complain(void);
%}

%%
"/*"               { TOKEN_START(); comment(); }
"//"[^\n]*         { /* consume //-comment */ }
^[ \t]*#.*[\r\n]   { /* ignore preprocessor directives (for now) */ }

"auto"             { TOKEN_START(); count(); return(AUTO); }
"_Bool"            { TOKEN_START(); count(); return(BOOL); }
"break"            { TOKEN_START(); count(); return(BREAK); }
"case"             { TOKEN_START(); count(); return(CASE); }
"char"             { TOKEN_START(); count(); return(CHAR); }
"_Complex"         { TOKEN_START(); count(); return(COMPLEX); }
"const"            { TOKEN_START(); count(); return(CONST); }
"continue"         { TOKEN_START(); count(); return(CONTINUE); }
"default"          { TOKEN_START(); count(); return(DEFAULT); }
"do"               { TOKEN_START(); count(); return(DO); }
"double"           { TOKEN_START(); count(); return(DOUBLE); }
"else"             { TOKEN_START(); count(); return(ELSE); }
"enum"             { TOKEN_START(); count(); return(ENUM); }
"extern"           { TOKEN_START(); count(); return(EXTERN); }
"float"            { TOKEN_START(); count(); return(FLOAT); }
"for"              { TOKEN_START(); count(); return(FOR); }
"goto"             { TOKEN_START(); count(); return(GOTO); }
"if"               { TOKEN_START(); count(); return(IF); }
"_Imaginary"       { TOKEN_START(); count(); return(IMAGINARY); }
"inline"           { TOKEN_START(); count(); return(INLINE); }
"asm"              { TOKEN_START(); count(); return(ASM); }
"__asm__"          { TOKEN_START(); count(); return(ASM); }
"int"              { TOKEN_START(); count(); return(INT); }
"long"             { TOKEN_START(); count(); return(LONG); }
"register"         { TOKEN_START(); count(); return(REGISTER); }
"__register__"     { TOKEN_START(); count(); return(REGISTER); }
"restrict"         { TOKEN_START(); count(); return(RESTRICT); }
"return"           { TOKEN_START(); count(); return(RETURN); }
"short"            { TOKEN_START(); count(); return(SHORT); }
"signed"           { TOKEN_START(); count(); return(SIGNED); }
"sizeof"           { TOKEN_START(); count(); return(SIZEOF); }
"static"           { TOKEN_START(); count(); return(STATIC); }
"struct"           { TOKEN_START(); count(); return(STRUCT); }
"switch"           { TOKEN_START(); count(); return(SWITCH); }
"typedef"          { TOKEN_START(); count(); return(TYPEDEF); }
"union"            { TOKEN_START(); count(); return(UNION); }
"unsigned"         { TOKEN_START(); count(); return(UNSIGNED); }
"void"             { TOKEN_START(); count(); return(VOID); }
"volatile"         { TOKEN_START(); count(); return(VOLATILE); }
"__volatile__"     { TOKEN_START(); count(); return(VOLATILE); }
"while"            { TOKEN_START(); count(); return(WHILE); }

{L}({L}|{D})*        { 
    count(); 
    int token = check_type();
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

{D}+{E}{FS}?                { TOKEN_START(); count(); yylval.int_val = strtoll(yytext, NULL, 10); return(CONSTANT); }
{D}*"."{D}+{E}?{FS}?        { TOKEN_START(); count(); yylval.int_val = (int64_t)strtod(yytext, NULL); return(CONSTANT); }
{D}+"."{D}*{E}?{FS}?        { TOKEN_START(); count(); yylval.int_val = (int64_t)strtod(yytext, NULL); return(CONSTANT); }
0[xX]{H}+{P}{FS}?           { TOKEN_START(); count(); yylval.int_val = strtoll(yytext, NULL, 16); return(CONSTANT); }
0[xX]{H}*"."{H}+{P}?{FS}?   { TOKEN_START(); count(); yylval.int_val = strtoll(yytext, NULL, 16); return(CONSTANT); }
0[xX]{H}+"."{H}*{P}?{FS}?   { TOKEN_START(); count(); yylval.int_val = strtoll(yytext, NULL, 16); return(CONSTANT); }


L?\"(\\.|[^\\"\n])*\"    { 
    count(); 
    yylval.str_val = nu_strdup(yytext);
    return(STRING_LITERAL); 
}

"..."            { TOKEN_START(); count(); return(ELLIPSIS); }
">>="            { TOKEN_START(); count(); return(RIGHT_ASSIGN); }
"<<="            { TOKEN_START(); count(); return(LEFT_ASSIGN); }
"+="             { TOKEN_START(); count(); return(ADD_ASSIGN); }
"-="             { TOKEN_START(); count(); return(SUB_ASSIGN); }
"*="             { TOKEN_START(); count(); return(MUL_ASSIGN); }
"/="             { TOKEN_START(); count(); return(DIV_ASSIGN); }
"%="             { TOKEN_START(); count(); return(MOD_ASSIGN); }
"&="             { TOKEN_START(); count(); return(AND_ASSIGN); }
"^="             { TOKEN_START(); count(); return(XOR_ASSIGN); }
"|="             { TOKEN_START(); count(); return(OR_ASSIGN); }
">>"             { TOKEN_START(); count(); return(RIGHT_OP); }
"<<"             { TOKEN_START(); count(); return(LEFT_OP); }
"++"             { TOKEN_START(); count(); return(INC_OP); }
"--"             { TOKEN_START(); count(); return(DEC_OP); }
"->"             { TOKEN_START(); count(); return(PTR_OP); }
"&&"             { TOKEN_START(); count(); return(AND_OP); }
"||"             { TOKEN_START(); count(); return(OR_OP); }
"<="             { TOKEN_START(); count(); return(LE_OP); }
">="             { TOKEN_START(); count(); return(GE_OP); }
"=="             { TOKEN_START(); count(); return(EQ_OP); }
"!="             { TOKEN_START(); count(); return(NE_OP); }
";"              { TOKEN_START(); count(); return(';'); }
("{"|"<%")       { TOKEN_START(); count(); return('{'); }
("}"|"%>")       { TOKEN_START(); count(); return('}'); }
","              { TOKEN_START(); count(); return(','); }
":"              { TOKEN_START(); count(); return(':'); }
"="              { TOKEN_START(); count(); return('='); }
"("              { TOKEN_START(); count(); return('('); }
")"              { TOKEN_START(); count(); return(')'); }
("["|"<:")       { TOKEN_START(); count(); return('['); }
("]"|":>")       { TOKEN_START(); count(); return(']'); }
"."              { TOKEN_START(); count(); return('.'); }
"&"              { TOKEN_START(); count(); return('&'); }
"!"              { TOKEN_START(); count(); return('!'); }
"~"              { TOKEN_START(); count(); return('~'); }
"-"              { TOKEN_START(); count(); return('-'); }
"+"              { TOKEN_START(); count(); return('+'); }
"*"              { TOKEN_START(); count(); return('*'); }
"/"              { TOKEN_START(); count(); return('/'); }
"%"              { TOKEN_START(); count(); return('%'); }
"<"              { TOKEN_START(); count(); return('<'); }
">"              { TOKEN_START(); count(); return('>'); }
"^"              { TOKEN_START(); count(); return('^'); }
"|"              { TOKEN_START(); count(); return('|'); }
"?"              { TOKEN_START(); count(); return('?'); }

[ \t\v\n\f]      { TOKEN_START(); count(); }
.                {
		    TOKEN_START();
		    char msg[64];
		    snprintf(msg, sizeof(msg), "unmatched character '%c'", yytext[0]);
		    cerror(msg);
		    count(); }

%%

void comment(void)
{
    int c;
    int prev = 0;

    while ((c = input()) != 0) {
        if (c == '\n') {
            line_no++;
            column = 1;
        } else if (c == '\t') {
            column += 8 - ((column - 1) % 8);
        } else {
            column++;
        }

        if (prev == '*' && c == '/')
            return;

        prev = c;
    }

    cerror("unterminated comment");
}


void count(void)
{
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            line_no++;
            column = 1;
        } else if (yytext[i] == '\t') {
            column += 8 - ((column - 1) % 8);
        } else {
            column++;
        }
    }
}

int check_type(void)
{
    if (is_registered_type(yytext)) {
        return TYPE_NAME; 
    }
    return IDENTIFIER;
}

int errcnt = 0;

void cerror(const char *s)
{
    fprintf(stderr, "%s:%d:%d: \x1b[91mError:\x1b[0m %s\n",
            current_filename ? current_filename : "<input>",
            tok_line, tok_col, s);

    // --err-limit=10 -> errlimt = 10, so wait 10 errors until exit
    // default errlimt = 3, so wait 3 errors until exit

    errcnt++;
    if (errcnt >= errlimt) {
      printf("\x1b[91mError:\x1b[0m Error Count reached!\n");
      printf("\x1b[32mTip:\x1b[0m Set -el or --error-limit to increase/decrease this limit.\n");
      exit(0);
    }
}

void complain(void)
{
    printf("W: Unmatched Characters!");
}
