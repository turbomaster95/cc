#ifndef TAB_H
#define TAB_H

#include <stdint.h>

typedef union {
    int64_t int_val;
    double float_val;
    const char* str_val;
    struct nu_ast_node *node;
} YYSTYPE;

extern YYSTYPE yylval;

enum TokenTypes {
    TOKEN_EOF = 0,
    
    // Primary/Literals
    IDENTIFIER = 258,
    CONSTANT = 259,
    STRING_LITERAL = 260,
    TYPE_NAME = 261,
    
    // Keywords
    SIZEOF = 262,
    TYPEDEF = 284,
    EXTERN = 285,
    STATIC = 286,
    AUTO = 287,
    REGISTER = 288,
    INLINE = 289,
    RESTRICT = 290,
    
    // Type specifiers
    VOID = 301,
    CHAR = 291,
    SHORT = 292,
    INT = 293,
    LONG = 294,
    SIGNED = 295,
    UNSIGNED = 296,
    FLOAT = 297,
    DOUBLE = 298,
    CONST = 299,
    VOLATILE = 300,
    BOOL = 302,
    COMPLEX = 303,
    IMAGINARY = 304,
    STRUCT = 305,
    UNION = 306,
    ENUM = 307,
    
    // Control flow
    CASE = 310,
    DEFAULT = 311,
    IF = 312,
    ELSE = 313,
    SWITCH = 314,
    WHILE = 315,
    DO = 316,
    FOR = 317,
    GOTO = 318,
    CONTINUE = 319,
    BREAK = 320,
    RETURN = 321,
    
    // Operators
    PTR_OP = 263,      // ->
    INC_OP = 264,      // ++
    DEC_OP = 265,      // --
    LEFT_OP = 266,     // <<
    RIGHT_OP = 267,    // >>
    LE_OP = 268,       // <=
    GE_OP = 269,       // >=
    EQ_OP = 270,       // ==
    NE_OP = 271,       // !=
    AND_OP = 272,      // &&
    OR_OP = 273,       // ||
    
    // Assignment Operators
    MUL_ASSIGN = 274,  // *=
    DIV_ASSIGN = 275,  // /=
    MOD_ASSIGN = 276,  // %=
    ADD_ASSIGN = 277,  // +=
    SUB_ASSIGN = 278,  // -=
    LEFT_ASSIGN = 279, // <<=
    RIGHT_ASSIGN = 280,// >>=
    AND_ASSIGN = 281,  // &=
    XOR_ASSIGN = 282,  // ^=
    OR_ASSIGN = 283,   // |=
    
    ELLIPSIS = 308,    // ...
    ASM = 309,          // inline assembly keyword
    ALIGN_OP = 322,
    GENERIC = 323,
    ATOMIC = 324,
    THREAD_LOCAL = 325,
    ALIGNAS = 326,
    NO_RETURN = 327,
    NORETURN = 327,
    STATIC_ASSERT = 328
};

#endif // TAB_H
