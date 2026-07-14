%code requires {
    #include <stdint.h>
    #include "compiler.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nu.h"

extern nu_mm_t *g_mm;
extern nu_ast_t *g_ast;

int yylex(void);
void yyerror(const char *s);
%}

%union {
    int64_t int_val;
    double float_val;
    const char* str_val;
    struct nu_ast_node *node; /* Every expression/statement yields an AST node pointer */
}

%token <str_val> IDENTIFIER STRING_LITERAL TYPE_NAME
%token <int_val> CONSTANT

%token SIZEOF
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token XOR_ASSIGN OR_ASSIGN

%token TYPEDEF EXTERN STATIC AUTO REGISTER INLINE RESTRICT
%token CHAR SHORT INT LONG SIGNED UNSIGNED FLOAT DOUBLE CONST VOLATILE VOID
%token BOOL COMPLEX IMAGINARY
%token STRUCT UNION ENUM ELLIPSIS

%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%type <node> argument_expression_list initializer_list
%type <node> primary_expression postfix_expression unary_expression cast_expression
%type <node> multiplicative_expression additive_expression shift_expression
%type <node> relational_expression equality_expression and_expression
%type <node> exclusive_or_expression inclusive_or_expression logical_and_expression
%type <node> logical_or_expression conditional_expression assignment_expression
%type <node> expression constant_expression statement labeled_statement
%type <node> compound_statement block_item_list block_item expression_statement
%type <node> selection_statement iteration_statement jump_statement
%type <node> translation_unit external_declaration function_definition
%type <node> declaration
%type <node> init_declarator_list init_declarator declarator direct_declarator initializer

%start translation_unit
%%

primary_expression
    : IDENTIFIER {
        $$ = nu_ast_new_node(g_ast, AST_IDENTIFIER);
        nu_ast_set_str(g_ast, $$, $1, strlen($1));
    }
    | CONSTANT {
        $$ = nu_ast_new_node(g_ast, AST_INTEGER_LITERAL);
        nu_ast_set_int($$, $1);
    }
    | STRING_LITERAL {
        $$ = nu_ast_new_node(g_ast, AST_STRING_LITERAL);
        nu_ast_set_str(g_ast, $$, $1, strlen($1));
    }
    | '(' expression ')' { $$ = $2; }
    ;

postfix_expression
    : primary_expression { $$ = $1; }
    | postfix_expression '[' expression ']' { 
        $$ = nu_ast_new_branch(g_ast, AST_ARRAY_REFERENCE, 2, $1, $3); 
    }
    | postfix_expression '(' ')' { 
        $$ = nu_ast_new_branch(g_ast, AST_FUNCTION_CALL, 1, $1); 
    }
    | postfix_expression '(' argument_expression_list ')' { 
        $$ = nu_ast_new_branch(g_ast, AST_FUNCTION_CALL, 2, $1, $3); 
    }
    | postfix_expression '.' IDENTIFIER { 
        nu_ast_node_t *id = nu_ast_new_node(g_ast, AST_IDENTIFIER);
        nu_ast_set_str(g_ast, id, $3, strlen($3));
        $$ = nu_ast_new_branch(g_ast, AST_MEMBER_ACCESS, 2, $1, id); 
    }
    | postfix_expression PTR_OP IDENTIFIER { 
        nu_ast_node_t *id = nu_ast_new_node(g_ast, AST_IDENTIFIER);
        nu_ast_set_str(g_ast, id, $3, strlen($3));
        $$ = nu_ast_new_branch(g_ast, AST_MEMBER_DEREF, 2, $1, id); 
    }
    | postfix_expression INC_OP { 
        $$ = nu_ast_new_branch(g_ast, AST_UNARY_POST_INC, 1, $1); 
    }
    | postfix_expression DEC_OP { 
        $$ = nu_ast_new_branch(g_ast, AST_UNARY_POST_DEC, 1, $1); 
    }
    | '(' type_name ')' '{' initializer_list '}' { 
        $$ = nu_ast_new_branch(g_ast, AST_INITIALIZER_LIST, 1, $5); 
    }
    | '(' type_name ')' '{' initializer_list ',' '}' { 
        $$ = nu_ast_new_branch(g_ast, AST_INITIALIZER_LIST, 1, $5); 
    }
    ;

argument_expression_list
    : assignment_expression { $$ = $1; }
    | argument_expression_list ',' assignment_expression {
        if ($1) {
            nu_ast_node_t *curr = $1;
            while (curr->next_sibling) {
                curr = curr->next_sibling;
            }
            curr->next_sibling = $3;
            $$ = $1;
        } else {
            $$ = $3;
        }
    }
    ;

unary_expression
    : postfix_expression { $$ = $1; }
    | INC_OP unary_expression { $$ = $2; }
    | DEC_OP unary_expression { $$ = $2; }
    | unary_operator cast_expression { $$ = $2; }
    | SIZEOF unary_expression { $$ = $2; }
    | SIZEOF '(' type_name ')' { $$ = NULL; }
    ;

unary_operator
    : '&' | '*' | '+' | '-' | '~' | '!'
    ;

cast_expression
    : unary_expression { $$ = $1; }
    | '(' type_name ')' cast_expression { $$ = $4; }
    ;

multiplicative_expression
    : cast_expression { $$ = $1; }
    | multiplicative_expression '*' cast_expression {
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_MUL, 2, $1, $3);
    }
    | multiplicative_expression '/' cast_expression {
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_DIV, 2, $1, $3);
    }
    | multiplicative_expression '%' cast_expression { $$ = $1; }
    ;

additive_expression
    : multiplicative_expression { $$ = $1; }
    | additive_expression '+' multiplicative_expression {
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_ADD, 2, $1, $3);
    }
    | additive_expression '-' multiplicative_expression {
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_SUB, 2, $1, $3);
    }
    ;

shift_expression
    : additive_expression { $$ = $1; }
    | shift_expression LEFT_OP additive_expression { $$ = $1; }
    | shift_expression RIGHT_OP additive_expression { $$ = $1; }
    ;

relational_expression
    : shift_expression { $$ = $1; }
    | relational_expression '<' shift_expression { 
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_LT, 2, $1, $3); 
    }
    | relational_expression '>' shift_expression { 
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_GT, 2, $1, $3); 
    }
    | relational_expression LE_OP shift_expression { 
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_LE, 2, $1, $3); 
    }
    | relational_expression GE_OP shift_expression { 
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_GE, 2, $1, $3); 
    }
    ;

equality_expression
    : relational_expression { $$ = $1; }
    | equality_expression EQ_OP relational_expression { 
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_EQ, 2, $1, $3); 
    }
    | equality_expression NE_OP relational_expression { 
        $$ = nu_ast_new_branch(g_ast, AST_BINARY_NE, 2, $1, $3); 
    }
    ;

and_expression
    : equality_expression { $$ = $1; }
    | and_expression '&' equality_expression { $$ = $1; }
    ;

exclusive_or_expression
    : and_expression { $$ = $1; }
    | exclusive_or_expression '^' and_expression { $$ = $1; }
    ;

inclusive_or_expression
    : exclusive_or_expression { $$ = $1; }
    | inclusive_or_expression '|' exclusive_or_expression { $$ = $1; }
    ;

logical_and_expression
    : inclusive_or_expression { $$ = $1; }
    | logical_and_expression AND_OP inclusive_or_expression { $$ = $1; }
    ;

logical_or_expression
    : logical_and_expression { $$ = $1; }
    | logical_or_expression OR_OP logical_and_expression { $$ = $1; }
    ;

conditional_expression
    : logical_or_expression { $$ = $1; }
    | logical_or_expression '?' expression ':' conditional_expression { $$ = $1; }
    ;

assignment_expression
    : conditional_expression { $$ = $1; }
    | unary_expression assignment_operator assignment_expression {
        $$ = nu_ast_new_branch(g_ast, AST_ASSIGN_EXPR, 2, $1, $3);
    }
    ;

assignment_operator
    : '=' | MUL_ASSIGN | DIV_ASSIGN | MOD_ASSIGN | ADD_ASSIGN | SUB_ASSIGN
    | LEFT_ASSIGN | RIGHT_ASSIGN | AND_ASSIGN | XOR_ASSIGN | OR_ASSIGN
    ;

expression
    : assignment_expression { $$ = $1; }
    | expression ',' assignment_expression { $$ = $1; }
    ;

constant_expression
    : conditional_expression { $$ = $1; }
    ;

declaration
    : declaration_specifiers ';' { $$ = NULL; }
    | declaration_specifiers init_declarator_list ';' { 
        $$ = nu_ast_new_node(g_ast, AST_DECLARATION); 
        if ($2) {
            nu_ast_add_child($$, $2);
        }
    }
    ;

declaration_specifiers
    : storage_class_specifier
    | storage_class_specifier declaration_specifiers
    | type_specifier
    | type_specifier declaration_specifiers
    | type_qualifier
    | type_qualifier declaration_specifiers
    | function_specifier
    | function_specifier declaration_specifiers
    ;

init_declarator_list
    : init_declarator { $$ = $1; }
    | init_declarator_list ',' init_declarator {
        if ($1) {
            struct nu_ast_node *curr = $1;
            while (curr->next_sibling) {
                curr = curr->next_sibling;
            }
            curr->next_sibling = $3;
            $$ = $1;
        } else {
            $$ = $3;
        }
    }
    ;

init_declarator
    : declarator { $$ = $1; }
    | declarator '=' initializer {
        $$ = nu_ast_new_branch(g_ast, AST_ASSIGN_EXPR, 2, $1, $3);
    }
    ;

storage_class_specifier
    : TYPEDEF | EXTERN | STATIC | AUTO | REGISTER
    ;

type_specifier
    : VOID | CHAR | SHORT | INT | LONG | FLOAT | DOUBLE | SIGNED | UNSIGNED | BOOL | COMPLEX | IMAGINARY
    | struct_or_union_specifier
    | enum_specifier
    | TYPE_NAME
    ;

struct_or_union_specifier
    : struct_or_union IDENTIFIER '{' struct_declaration_list '}'
    | struct_or_union '{' struct_declaration_list '}'
    | struct_or_union IDENTIFIER
    ;

struct_or_union
    : STRUCT | UNION
    ;

struct_declaration_list
    : struct_declaration
    | struct_declaration_list struct_declaration
    ;

struct_declaration
    : specifier_qualifier_list struct_declarator_list ';'
    ;

specifier_qualifier_list
    : type_specifier specifier_qualifier_list
    | type_specifier
    | type_qualifier specifier_qualifier_list
    | type_qualifier
    ;

struct_declarator_list
    : struct_declarator
    | struct_declarator_list ',' struct_declarator
    ;

struct_declarator
    : declarator
    | ':' constant_expression
    | declarator ':' constant_expression
    ;

enum_specifier
    : ENUM '{' enumerator_list '}'
    | ENUM IDENTIFIER '{' enumerator_list '}'
    | ENUM '{' enumerator_list ',' '}'
    | ENUM IDENTIFIER '{' enumerator_list ',' '}'
    | ENUM IDENTIFIER
    ;

enumerator_list
    : enumerator
    | enumerator_list ',' enumerator
    ;

enumerator
    : IDENTIFIER
    | IDENTIFIER '=' constant_expression
    ;

type_qualifier
    : CONST | RESTRICT | VOLATILE
    ;

function_specifier
    : INLINE
    ;

declarator
    : pointer direct_declarator { $$ = $2; }
    | direct_declarator { $$ = $1; }
    ;

direct_declarator
    : IDENTIFIER {
        $$ = nu_ast_new_node(g_ast, AST_IDENTIFIER);
        nu_ast_set_str(g_ast, $$, $1, strlen($1));
    }
    | '(' declarator ')' { $$ = $2; }
    | direct_declarator '[' type_qualifier_list assignment_expression ']' { $$ = $1; }
    | direct_declarator '[' type_qualifier_list ']' { $$ = $1; }
    | direct_declarator '[' assignment_expression ']' { $$ = $1; }
    | direct_declarator '[' STATIC type_qualifier_list assignment_expression ']' { $$ = $1; }
    | direct_declarator '[' type_qualifier_list STATIC assignment_expression ']' { $$ = $1; }
    | direct_declarator '[' type_qualifier_list '*' ']' { $$ = $1; }
    | direct_declarator '[' '*' ']' { $$ = $1; }
    | direct_declarator '[' ']' { $$ = $1; }
    | direct_declarator '(' parameter_type_list ')' { $$ = $1; }
    | direct_declarator '(' identifier_list ')' { $$ = $1; }
    | direct_declarator '(' ')' { $$ = $1; }
    ;

pointer
    : '*'
    | '*' type_qualifier_list
    | '*' pointer
    | '*' type_qualifier_list pointer
    ;

type_qualifier_list
    : type_qualifier
    | type_qualifier_list type_qualifier
    ;

parameter_type_list
    : parameter_list
    | parameter_list ',' ELLIPSIS
    ;

parameter_list
    : parameter_declaration
    | parameter_list ',' parameter_declaration
    ;

parameter_declaration
    : declaration_specifiers declarator
    | declaration_specifiers abstract_declarator
    | declaration_specifiers
    ;

identifier_list
    : IDENTIFIER
    | identifier_list ',' IDENTIFIER
    ;

type_name
    : specifier_qualifier_list
    | specifier_qualifier_list abstract_declarator
    ;

abstract_declarator
    : pointer
    | direct_abstract_declarator
    | pointer direct_abstract_declarator
    ;

direct_abstract_declarator
    : '(' abstract_declarator ')'
    | '[' ']'
    | '[' assignment_expression ']'
    | direct_abstract_declarator '[' ']'
    | direct_abstract_declarator '[' assignment_expression ']'
    | '[' '*' ']'
    | direct_abstract_declarator '[' '*' ']'
    | '(' ')'
    | '(' parameter_type_list ')'
    | direct_abstract_declarator '(' ')'
    | direct_abstract_declarator '(' parameter_type_list ')'
    ;

initializer
    : assignment_expression { $$ = $1; }
    | '{' initializer_list '}' { $$ = $2; }
    | '{' initializer_list ',' '}' { $$ = $2; }
    ;

initializer_list
    : initializer { 
        $$ = $1; 
    }
    | designation initializer { 
        $$ = $2; 
    }
    | initializer_list ',' initializer {
        if ($1 && $3) {
            nu_ast_node_t *curr = $1;
            while (curr->next_sibling) {
                curr = curr->next_sibling;
            }
            curr->next_sibling = $3;
            $$ = $1;
        } else {
            $$ = $1 ? $1 : $3;
        }
    }
    | initializer_list ',' designation initializer {
        if ($1 && $4) {
            nu_ast_node_t *curr = $1;
            while (curr->next_sibling) {
                curr = curr->next_sibling;
            }
            curr->next_sibling = $4;
            $$ = $1;
        } else {
            $$ = $1 ? $1 : $4;
        }
    }
    ;

designation
    : designator_list '='
    ;

designator_list
    : designator
    | designator_list designator
    ;

designator
    : '[' constant_expression ']'
    | '.' IDENTIFIER
    ;

statement
    : labeled_statement { $$ = $1; }
    | compound_statement { $$ = $1; }
    | expression_statement { $$ = $1; }
    | selection_statement { $$ = $1; }
    | iteration_statement { $$ = $1; }
    | jump_statement { $$ = $1; }
    ;

labeled_statement
    : IDENTIFIER ':' statement { $$ = $3; }
    | CASE constant_expression ':' statement { $$ = $4; }
    | DEFAULT ':' statement { $$ = $3; }
    ;

compound_statement
    : '{' '}' { 
        $$ = nu_ast_new_node(g_ast, AST_COMPOUND_STATEMENT); 
    }
    | '{' block_item_list '}' {
        $$ = nu_ast_new_branch(g_ast, AST_COMPOUND_STATEMENT, 1, $2);
    }
    ;

block_item_list
    : block_item { $$ = $1; }
    | block_item_list block_item {
        if ($1) {
            nu_ast_node_t *curr = $1;
            while (curr->next_sibling) {
                curr = curr->next_sibling;
            }
            curr->next_sibling = $2;
            $$ = $1;
        } else {
            $$ = $2;
        }
    }
    ;

block_item
    : declaration { $$ = $1; }
    | statement { $$ = $1; }
    ;

expression_statement
    : ';' { $$ = NULL; }
    | expression ';' { $$ = $1; }
    ;

selection_statement
    : IF '(' expression ')' statement {
        $$ = nu_ast_new_branch(g_ast, AST_IF_STATEMENT, 2, $3, $5);
    }
    | IF '(' expression ')' statement ELSE statement {
        $$ = nu_ast_new_branch(g_ast, AST_IF_STATEMENT, 3, $3, $5, $7);
    }
    | SWITCH '(' expression ')' statement { $$ = $5; }
    ;

iteration_statement
    : WHILE '(' expression ')' statement {
        $$ = nu_ast_new_branch(g_ast, AST_WHILE_STATEMENT, 2, $3, $5);
    }
    | DO statement WHILE '(' expression ')' ';' { $$ = $2; }
    | FOR '(' expression_statement expression_statement ')' statement { $$ = $6; }
    | FOR '(' expression_statement expression_statement expression ')' statement { $$ = $7; }
    | FOR '(' declaration expression_statement ')' statement { $$ = $6; }
    | FOR '(' declaration expression_statement expression ')' statement { $$ = $7; }
    ;

jump_statement
    : GOTO IDENTIFIER ';' { $$ = NULL; }
    | CONTINUE ';' { $$ = NULL; }
    | BREAK ';' { $$ = NULL; }
    | RETURN ';' {
        $$ = nu_ast_new_node(g_ast, AST_RETURN_STATEMENT);
    }
    | RETURN expression ';' {
        $$ = nu_ast_new_branch(g_ast, AST_RETURN_STATEMENT, 1, $2);
    }
    ;

translation_unit
    : external_declaration {
        g_ast->root = nu_ast_new_branch(g_ast, AST_TRANSLATION_UNIT, 1, $1);
        $$ = g_ast->root;
    }
    | translation_unit external_declaration {
        if ($2) {
            nu_ast_add_child(g_ast->root, $2);
        }
        $$ = g_ast->root;
    }
    ;

external_declaration
    : function_definition { $$ = $1; }
    | declaration { $$ = $1; }
    ;

function_definition
    : declaration_specifiers declarator declaration_list compound_statement {
        $$ = nu_ast_new_branch(g_ast, AST_FUNCTION_DEF, 2, $2, $4);
    }
    | declaration_specifiers declarator compound_statement {
        $$ = nu_ast_new_branch(g_ast, AST_FUNCTION_DEF, 2, $2, $3);
    }

declaration_list
    : declaration
    | declaration_list declaration
    ;

%%
extern char yytext[];
extern int column;

void yyerror(char const *s)
{
    fflush(stdout);
    printf("\n%*s\n%*s\n", column, "^", column, s);
}
