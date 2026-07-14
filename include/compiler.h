#ifndef COMPILER_H
#define COMPILER_H

#include <nu.h>

enum DSNodeTypes {
    /* Root & Top-Level Construction */
    AST_TRANSLATION_UNIT = 1000,
    AST_FUNCTION_DEF,
    AST_DECLARATION,
    AST_DECLARATION_SPECIFIERS,
    AST_INIT_DECLARATOR_LIST,
    
    /* Statements */
    AST_COMPOUND_STATEMENT,
    AST_EXPRESSION_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_DO_WHILE_STATEMENT,
    AST_FOR_STATEMENT,
    AST_SWITCH_STATEMENT,
    AST_CASE_STATEMENT,
    AST_DEFAULT_STATEMENT,
    AST_LABEL_STATEMENT,
    AST_GOTO_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_BREAK_STATEMENT,

    /* Expressions & Operators */
    AST_ASSIGN_EXPR,
    AST_COMMA_EXPR,
    AST_CONDITIONAL_EXPR,    /* The ternary operator `? :` */
    
    /* Binary Operators */
    AST_BINARY_ADD,
    AST_BINARY_SUB,
    AST_BINARY_MUL,
    AST_BINARY_DIV,
    AST_BINARY_MOD,
    AST_BINARY_LSHIFT,       /* `<<` */
    AST_BINARY_RSHIFT,       /* `>>` */
    AST_BINARY_LT,           /* `<` */
    AST_BINARY_GT,           /* `>` */
    AST_BINARY_LE,           /* `<=` */
    AST_BINARY_GE,           /* `>=` */
    AST_BINARY_EQ,           /* `==` */
    AST_BINARY_NE,           /* `!=` */
    AST_BINARY_BIT_AND,      /* `&` */
    AST_BINARY_BIT_XOR,      /* `^` */
    AST_BINARY_BIT_OR,       /* `|` */
    AST_BINARY_LOGIC_AND,    /* `&&` */
    AST_BINARY_LOGIC_OR,     /* `||` */

    /* Compound Assignment Operators */
    AST_ASSIGN_MUL,          /* `*=` */
    AST_ASSIGN_DIV,          /* `/=` */
    AST_ASSIGN_MOD,          /* `%=` */
    AST_ASSIGN_ADD,          /* `+=` */
    AST_ASSIGN_SUB,          /* `-=` */
    AST_ASSIGN_LSHIFT,       /* `<<=` */
    AST_ASSIGN_RSHIFT,       /* `>>=` */
    AST_ASSIGN_AND,          /* `&=` */
    AST_ASSIGN_XOR,          /* `^=` */
    AST_ASSIGN_OR,           /* `|=` */

    /* Unary, Postfix, & Primary Prefix Operators */
    AST_UNARY_PRE_INC,       /* `++x` */
    AST_UNARY_PRE_DEC,       /* `--x` */
    AST_UNARY_POST_INC,      /* `x++` */
    AST_UNARY_POST_DEC,      /* `x--` */
    AST_UNARY_ADDRESS,       /* `&x` */
    AST_UNARY_DEREFERENCE,   /* `*x` */
    AST_UNARY_PLUS,          /* `+x` */
    AST_UNARY_MINUS,         /* `-x` */
    AST_UNARY_BIT_NOT,       /* `~x` */
    AST_UNARY_LOGIC_NOT,     /* `!x` */
    AST_SIZEOF_EXPR,
    AST_SIZEOF_TYPE,
    AST_CAST_EXPR,

    /* Postfix Structural Operations */
    AST_ARRAY_REFERENCE,     /* `x[i]` */
    AST_FUNCTION_CALL,       /* `f(args)` */
    AST_MEMBER_ACCESS,       /* `x.y` */
    AST_MEMBER_DEREF,        /* `x->y` */
    
    /* Literals & Terminals */
    AST_IDENTIFIER,
    AST_INTEGER_LITERAL,
    AST_FLOAT_LITERAL,
    AST_STRING_LITERAL,
    AST_TYPE_NAME,

    /* Specifiers, Qualifiers, & Typings */
    AST_TYPE_SPECIFIER,      /* void, char, int, etc. */
    AST_STORAGE_CLASS,       /* typedef, extern, static, auto, register, inline */
    AST_TYPE_QUALIFIER,      /* const, restrict, volatile */

    /* Complex Aggregate Structures */
    AST_STRUCT_SPECIFIER,
    AST_UNION_SPECIFIER,
    AST_ENUM_SPECIFIER,
    AST_ENUMERATOR,
    AST_STRUCT_DECLARATION,
    AST_STRUCT_DECLARATOR,
    
    /* Declarators & Initialization */
    AST_POINTER_DECLARATOR,
    AST_ARRAY_DECLARATOR,
    AST_FUNCTION_DECLARATOR,
    AST_PARAMETER_LIST,
    AST_PARAMETER_DECLARATION,
    AST_INITIALIZER_LIST,
    AST_DESIGNATOR,           /* Designated initializers like `[.x = 1]` */
    AST_INLINE_ASM
};

#endif // COMPILER_H
