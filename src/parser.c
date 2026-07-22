#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <compiler.h>
#include <nu.h>
#include <tab.h>

extern int yylex(void);
extern char *yytext;
extern int column;

YYSTYPE yylval;

extern nu_mm_t *g_mm;
extern nu_ast_t *g_ast;
extern int g_c11_enabled;
extern int is_registered_type(const char *name);
extern void add_type(const char *name);
extern void parser_init_hook(void);
extern void register_struct(const char *name, nu_ast_node_t *member_list);

typedef struct {
    int token;
    YYSTYPE val;
    char text[1024];
} Token;

typedef struct {
    Token current;
    Token peek;
} Parser;

static Parser p;
static nu_map_t *kw_map = NULL;
static nu_map_t *enum_const_map = NULL;

typedef enum {
    KW_NONE      = 0,
    KW_TYPE_SPEC = 1 << 0,
    KW_STORAGE   = 1 << 1,
    KW_QUALIFIER = 1 << 2,
    KW_ALIGN     = 1 << 3,
    KW_C11       = 1 << 4
} KeywordFlag;

static void add_kw(const char *kw, KeywordFlag flags) {
    nu_map_set(kw_map, kw, (void*)(uintptr_t)flags);
}

static KeywordFlag get_kw_flags(void) {
    if (!kw_map) return KW_NONE;
    uintptr_t flags = (uintptr_t)nu_map_get(kw_map, p.current.text);
    if ((flags & KW_C11) && !g_c11_enabled) return KW_NONE;
    return (KeywordFlag)flags;
}

void yyerror(const char *s) {
    fprintf(stderr, "error: %s", s);
    if (p.current.text[0] != '\0' && p.current.token != 0) {
        fprintf(stderr, " before '%s'", p.current.text);
    }
    fprintf(stderr, " (col %d)\n", column);
    if (column > 0) {
        fprintf(stderr, "%*s\n", column, "^");
    }
}

static void advance(void) {
    p.current = p.peek;
    p.peek.token = yylex();
    p.peek.val = yylval;

    if (yytext) {
        strncpy(p.peek.text, yytext, sizeof(p.peek.text) - 1);
        p.peek.text[sizeof(p.peek.text) - 1] = '\0';
    } else {
        p.peek.text[0] = '\0';
    }
}

static int64_t ast_get_int(nu_ast_node_t *node) {
    if (!node) return 0;
    return node->val.i64;
}

static void init_parser(void) {
    kw_map = nu_map_create(g_mm, 64);
    parser_init_hook();
    enum_const_map = nu_map_create(g_mm, 64);

    // Core Types
    add_kw("void", KW_TYPE_SPEC); add_kw("char", KW_TYPE_SPEC);
    add_kw("short", KW_TYPE_SPEC); add_kw("int", KW_TYPE_SPEC);
    add_kw("long", KW_TYPE_SPEC); add_kw("float", KW_TYPE_SPEC);
    add_kw("double", KW_TYPE_SPEC); add_kw("signed", KW_TYPE_SPEC);
    add_kw("unsigned", KW_TYPE_SPEC); add_kw("_Bool", KW_TYPE_SPEC);
    add_kw("_Complex", KW_TYPE_SPEC); add_kw("_Imaginary", KW_TYPE_SPEC);
    add_kw("struct", KW_TYPE_SPEC); add_kw("union", KW_TYPE_SPEC);
    add_kw("enum", KW_TYPE_SPEC);

    // Qualifiers
    add_kw("const", KW_QUALIFIER); add_kw("volatile", KW_QUALIFIER);
    add_kw("restrict", KW_QUALIFIER); add_kw("inline", KW_QUALIFIER);

    // Storage classes
    add_kw("typedef", KW_STORAGE); add_kw("extern", KW_STORAGE);
    add_kw("static", KW_STORAGE); add_kw("auto", KW_STORAGE);
    add_kw("register", KW_STORAGE);

    // C11 Features
    add_kw("_Atomic", KW_TYPE_SPEC | KW_QUALIFIER | KW_C11);
    add_kw("_Thread_local", KW_STORAGE | KW_C11);
    add_kw("_Noreturn", KW_QUALIFIER | KW_C11);
    add_kw("_Alignas", KW_ALIGN | KW_C11);

    advance();
    advance();
}

static int match(int token_type) {
    return p.current.token == token_type;
}

static void consume(int token_type, const char *err_msg) {
    if (match(token_type)) {
        advance();
    } else {
        yyerror(err_msg);
        exit(EXIT_FAILURE);
    }
}

static char* dup_string(const char *str) {
    char *copy = nu_alloc(g_mm, strlen(str) + 1);
    if (!copy) {
        perror("dup_string: out of memory");
        exit(EXIT_FAILURE);
    }
    return strcpy(copy, str);
}

static nu_ast_node_t* link_sibling(nu_ast_node_t *head, nu_ast_node_t *next) {
    if (!head) return next;
    if (!next) return head;
    nu_ast_node_t *curr = head;
    while (curr->next_sibling) {
        curr = curr->next_sibling;
    }
    curr->next_sibling = next;
    return head;
}

typedef enum {
    PREC_NONE, PREC_COMMA, PREC_ASSIGN, PREC_CONDITIONAL,
    PREC_LOGICAL_OR, PREC_LOGICAL_AND, PREC_OR, PREC_XOR,
    PREC_AND, PREC_EQUALITY, PREC_COMPARISON, PREC_SHIFT,
    PREC_ADDITIVE, PREC_MULTIPLICATIVE, PREC_UNARY, PREC_POSTFIX,
    PREC_PRIMARY
} Precedence;

typedef nu_ast_node_t* (*PrefixFn)(void);
typedef nu_ast_node_t* (*InfixFn)(nu_ast_node_t* left);

typedef struct {
    PrefixFn prefix;
    InfixFn infix;
    Precedence precedence;
} ParseRule;

static nu_ast_node_t* parse_expr_with_precedence(Precedence prec);
static const ParseRule* get_rule(int token_type);
static nu_ast_node_t* parse_declarator(void);
static int is_type_specifier_start(void);
static int parse_declaration_specifiers(void);
static nu_ast_node_t* parse_declaration(void);
static nu_ast_node_t* parse_statement(void);

static const char* find_identifier_str(nu_ast_node_t *node) {
    if (!node) return NULL;
    if (node->type == AST_IDENTIFIER && node->val.str) {
        return node->val.str;
    }
    for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
        const char *s = find_identifier_str(c);
        if (s) return s;
    }
    return NULL;
}

static nu_ast_node_t* parse_initializer(void) {
    if (match('{')) {
        advance();
        nu_ast_node_t *init_list = NULL;
        while (!match('}') && !match(TOKEN_EOF)) {
            while (match('.') || match('[')) {
                if (match('.')) {
                    advance();
                    if (match(IDENTIFIER) || match(TYPE_NAME)) {
                        advance();
                    }
                } else if (match('[')) {
                    advance();
                    parse_expr_with_precedence(PREC_COMMA);
                    consume(']', "Expected ']' after array designator");
                }
                if (match('=')) {
                    advance();
                }
            }

            nu_ast_node_t *sub_init = parse_initializer();
            init_list = link_sibling(init_list, sub_init);

            if (match(',')) {
                advance();
            } else {
                break;
            }
        }
        consume('}', "Expected closing '}' in initializer");
        return init_list;
    } else {
        return parse_expr_with_precedence(PREC_ASSIGN);
    }
}

static nu_ast_node_t* prefix_identifier(void) {
    char *name = p.current.text;
    if (enum_const_map) {
        uintptr_t stored = (uintptr_t)nu_map_get(enum_const_map, name);
        if (stored != 0) {
            long val = (long)stored - 1;
            advance();
            nu_ast_node_t *node = nu_ast_new_node(g_ast, AST_INTEGER_LITERAL);
            nu_ast_set_int(node, val);
            return node;
        }
    }
    nu_ast_node_t *node = nu_ast_new_node(g_ast, AST_IDENTIFIER);
    char *saved_name = dup_string(name);
    nu_ast_set_str(g_ast, node, saved_name, strlen(saved_name));
    advance();
    return node;
}

static nu_ast_node_t* prefix_constant(void) {
    nu_ast_node_t *node = nu_ast_new_node(g_ast, AST_INTEGER_LITERAL);
    nu_ast_set_int(node, p.current.val.int_val);
    advance();
    return node;
}

static nu_ast_node_t* prefix_string(void) {
    nu_ast_node_t *node = nu_ast_new_node(g_ast, AST_STRING_LITERAL);
    nu_ast_set_str(g_ast, node, p.current.text + 1, strlen(p.current.text) - 2);
    advance();
    return node;
}

static nu_ast_node_t* parse_for_statement(void) {
    consume(FOR, "Expected 'for'");
    consume('(', "Expected '(' after 'for'");

    nu_ast_node_t *init_node = NULL;
    if (!match(';')) {
        if (is_type_specifier_start()) {
            init_node = parse_declaration();
        } else {
            init_node = parse_expr_with_precedence(PREC_COMMA);
            consume(';', "Expected ';' after for-loop initializer");
        }
    } else {
        consume(';', "Expected ';'");
    }

    nu_ast_node_t *cond_node = NULL;
    if (!match(';')) {
        cond_node = parse_expr_with_precedence(PREC_COMMA);
    }
    consume(';', "Expected ';' after for-loop condition");

    nu_ast_node_t *inc_node = NULL;
    if (!match(')')) {
        inc_node = parse_expr_with_precedence(PREC_COMMA);
    }
    consume(')', "Expected ')' after for-loop increment");

    nu_ast_node_t *body_node = parse_statement();

    return nu_ast_new_branch(g_ast, AST_FOR_STATEMENT, 4, init_node, cond_node, inc_node, body_node);
}

static nu_ast_node_t* prefix_grouping(void) {
    advance();
    if (is_type_specifier_start()) {
        parse_declaration_specifiers();
        nu_ast_node_t *type_decl = parse_declarator();
        consume(')', "Expected matching ')' after compound literal type cast");
        if (match('{')) {
            advance();
            nu_ast_node_t *init_list = NULL;
            while (!match('}') && !match(TOKEN_EOF)) {
                while (match('.') || match('[')) {
                    if (match('.')) {
                        advance();
                        if (match(IDENTIFIER) || match(TYPE_NAME)) advance();
                    } else if (match('[')) {
                        advance();
                        parse_expr_with_precedence(PREC_COMMA);
                        consume(']', "Expected ']' in designator index");
                    }
                    if (match('=')) advance();
                }
                nu_ast_node_t *sub_init = parse_initializer();
                init_list = link_sibling(init_list, sub_init);
                if (match(',')) advance();
                else break;
            }
            consume('}', "Expected closing '}' at end of compound literal");
            return nu_ast_new_branch(g_ast, AST_COMPOUND_LITERAL, 2, type_decl, init_list);
        }
        return type_decl;
    }
    nu_ast_node_t *node = parse_expr_with_precedence(PREC_COMMA);
    consume(')', "Expected matching ')'");
    return node;
}

static nu_ast_node_t* prefix_unary(void) {
    int op = p.current.token;
    advance();
    nu_ast_node_t *operand = parse_expr_with_precedence(PREC_UNARY);
    int type = AST_NONE;
    switch (op) {
        case '&': type = AST_UNARY_ADDRESS; break;
        case '*': type = AST_UNARY_DEREFERENCE; break;
        case '+': type = AST_UNARY_PLUS; break;
        case '-': type = AST_UNARY_MINUS; break;
        case '~': type = AST_UNARY_BIT_NOT; break;
        case '!': type = AST_UNARY_LOGIC_NOT; break;
        case INC_OP: type = AST_UNARY_PRE_INC; break;
        case DEC_OP: type = AST_UNARY_PRE_DEC; break;
        default: return operand;
    }
    return nu_ast_new_branch(g_ast, type, 1, operand);
}

static nu_ast_node_t* prefix_alignof(void) {
    if (!g_c11_enabled) {
        yyerror("alignof / _Alignof is a C11-only feature.");
        exit(EXIT_FAILURE);
    }
    advance();
    consume('(', "Expected '(' after alignof");
    nu_ast_node_t *type_decl = parse_declarator();
    consume(')', "Expected matching ')'");
    nu_ast_node_t *node = nu_ast_new_node(g_ast, AST_ALIGNOF_EXPR);
    nu_ast_add_child(node, type_decl);
    return node;
}

static nu_ast_node_t* prefix_generic(void) {
    if (!g_c11_enabled) {
        yyerror("_Generic is a C11-only feature.");
        exit(EXIT_FAILURE);
    }
    advance();
    consume('(', "Expected '(' after _Generic");
    nu_ast_node_t *controlling_expr = parse_expr_with_precedence(PREC_ASSIGN);
    consume(',', "Expected ',' after generic controlling expression");

    nu_ast_node_t *assoc_list = NULL;
    while (1) {
        nu_ast_node_t *assoc = nu_ast_new_node(g_ast, AST_GENERIC_ASSOCIATION);
        if (match(DEFAULT)) {
            nu_ast_node_t *def_node = nu_ast_new_node(g_ast, AST_IDENTIFIER);
            nu_ast_set_str(g_ast, def_node, "default", 7);
            nu_ast_add_child(assoc, def_node);
            advance();
        } else {
            nu_ast_node_t *type_name = parse_declarator();
            nu_ast_add_child(assoc, type_name);
        }
        consume(':', "Expected ':' after generic association selector");
        nu_ast_node_t *expr = parse_expr_with_precedence(PREC_ASSIGN);
        nu_ast_add_child(assoc, expr);
        assoc_list = link_sibling(assoc_list, assoc);

        if (match(',')) {
            advance();
        } else {
            break;
        }
    }
    consume(')', "Expected matching ')'");
    return nu_ast_new_branch(g_ast, AST_GENERIC_SELECTION, 2, controlling_expr, assoc_list);
}

static nu_ast_node_t* infix_binary(nu_ast_node_t* left) {
    int op = p.current.token;
    const ParseRule *rule = get_rule(op);
    advance();
    nu_ast_node_t *right = parse_expr_with_precedence((Precedence)(rule->precedence + 1));
    int type = AST_NONE;
    switch (op) {
        case '+': type = AST_BINARY_ADD; break;
        case '-': type = AST_BINARY_SUB; break;
        case '*': type = AST_BINARY_MUL; break;
        case '/': type = AST_BINARY_DIV; break;
        case '<': type = AST_BINARY_LT;  break;
        case '>': type = AST_BINARY_GT;  break;
        case '|': type = AST_BINARY_BIT_OR; break;
        case '^': type = AST_BINARY_BIT_XOR; break;
        case '&': type = AST_BINARY_BIT_AND; break;
        case LE_OP: type = AST_BINARY_LE; break;
        case GE_OP: type = AST_BINARY_GE; break;
        case EQ_OP: type = AST_BINARY_EQ; break;
        case NE_OP: type = AST_BINARY_NE; break;
        case AND_OP: type = AST_BINARY_LOGIC_AND; break;
        case OR_OP:  type = AST_BINARY_LOGIC_OR;  break;
        case LEFT_OP:  type = AST_BINARY_SHL; break;
        case RIGHT_OP: type = AST_BINARY_SHR; break;
        default: return left;
    }
    return nu_ast_new_branch(g_ast, type, 2, left, right);
}

static nu_ast_node_t* clone_ast_node(nu_ast_node_t *node) {
    if (!node) return NULL;
    nu_ast_node_t *copy = nu_ast_new_node(g_ast, node->type);
    copy->val = node->val;
    if (node->val.str) {
        copy->val.str = dup_string(node->val.str);
    }
    for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
        nu_ast_add_child(copy, clone_ast_node(c));
    }
    return copy;
}

static nu_ast_node_t* infix_assignment(nu_ast_node_t* left) {
    int op = p.current.token;
    advance();
    nu_ast_node_t *right = parse_expr_with_precedence(PREC_ASSIGN);
    if (op != '=') {
        int base_type = AST_NONE;
        switch (op) {
            case ADD_ASSIGN: base_type = AST_BINARY_ADD; break;
            case SUB_ASSIGN: base_type = AST_BINARY_SUB; break;
            case MUL_ASSIGN: base_type = AST_BINARY_MUL; break;
            case DIV_ASSIGN: base_type = AST_BINARY_DIV; break;
            case AND_ASSIGN: base_type = AST_BINARY_BIT_AND; break;
            case OR_ASSIGN:  base_type = AST_BINARY_BIT_OR;  break;
            case XOR_ASSIGN: base_type = AST_BINARY_BIT_XOR; break;
        }
        right = nu_ast_new_branch(g_ast, base_type, 2, clone_ast_node(left), right);
    }
    return nu_ast_new_branch(g_ast, AST_ASSIGN_EXPR, 2, left, right);
}

static nu_ast_node_t* postfix_call(nu_ast_node_t* left) {
    advance();
    if (match(')')) {
        advance();
        return nu_ast_new_branch(g_ast, AST_FUNCTION_CALL, 1, left);
    }
    nu_ast_node_t *args = NULL;
    while (1) {
        args = link_sibling(args, parse_expr_with_precedence(PREC_ASSIGN));
        if (match(',')) advance();
        else break;
    }
    consume(')', "Expected ')' at end of call");
    return nu_ast_new_branch(g_ast, AST_FUNCTION_CALL, 2, left, args);
}

static nu_ast_node_t* postfix_subscript(nu_ast_node_t* left) {
    advance();
    nu_ast_node_t *index = parse_expr_with_precedence(PREC_COMMA);
    consume(']', "Expected closing ']'");
    return nu_ast_new_branch(g_ast, AST_ARRAY_REFERENCE, 2, left, index);
}

static nu_ast_node_t* postfix_member(nu_ast_node_t* left) {
    int op = p.current.token;
    advance();
    char *saved_name = dup_string(p.current.text);
    consume(IDENTIFIER, "Expected member identifier name");
    nu_ast_node_t *id = nu_ast_new_node(g_ast, AST_IDENTIFIER);
    nu_ast_set_str(g_ast, id, saved_name, strlen(saved_name));
    return nu_ast_new_branch(g_ast, (op == '.') ? AST_MEMBER_ACCESS : AST_MEMBER_DEREF, 2, left, id);
}

static nu_ast_node_t* postfix_increment(nu_ast_node_t* left) {
    int op = p.current.token;
    advance();
    return nu_ast_new_branch(g_ast, (op == INC_OP) ? AST_UNARY_POST_INC : AST_UNARY_POST_DEC, 1, left);
}

static const ParseRule rules[] = {
    [IDENTIFIER]     = { prefix_identifier, NULL,              PREC_NONE },
    [CONSTANT]       = { prefix_constant,   NULL,              PREC_NONE },
    [STRING_LITERAL] = { prefix_string,     NULL,              PREC_NONE },
    ['(']            = { prefix_grouping,   postfix_call,      PREC_POSTFIX },
    ['[']            = { NULL,              postfix_subscript, PREC_POSTFIX },
    ['.']            = { NULL,              postfix_member,    PREC_POSTFIX },
    [PTR_OP]         = { NULL,              postfix_member,    PREC_POSTFIX },
    [INC_OP]         = { prefix_unary,      postfix_increment, PREC_POSTFIX },
    [DEC_OP]         = { prefix_unary,      postfix_increment, PREC_POSTFIX },
    ['+']            = { prefix_unary,      infix_binary,      PREC_ADDITIVE },
    ['-']            = { prefix_unary,      infix_binary,      PREC_ADDITIVE },
    ['*']            = { prefix_unary,      infix_binary,      PREC_MULTIPLICATIVE },
    ['/']            = { prefix_unary,      infix_binary,      PREC_MULTIPLICATIVE },
    ['&']            = { prefix_unary,      infix_binary,      PREC_AND },
    ['^']            = { NULL,              infix_binary,      PREC_XOR },
    ['|']            = { NULL,              infix_binary,      PREC_OR },
    ['~']            = { prefix_unary,      NULL,              PREC_NONE },
    ['!']            = { prefix_unary,      NULL,              PREC_NONE },
    ['<']            = { NULL,              infix_binary,      PREC_COMPARISON },
    ['>']            = { NULL,              infix_binary,      PREC_COMPARISON },
    [LE_OP]          = { NULL,              infix_binary,      PREC_COMPARISON },
    [GE_OP]          = { NULL,              infix_binary,      PREC_COMPARISON },
    [EQ_OP]          = { NULL,              infix_binary,      PREC_EQUALITY },
    [NE_OP]          = { NULL,              infix_binary,      PREC_EQUALITY },
    [AND_OP]         = { NULL,              infix_binary,      PREC_LOGICAL_AND },
    [OR_OP]          = { NULL,              infix_binary,      PREC_LOGICAL_OR },
    [LEFT_OP]        = { NULL,              infix_binary,      PREC_SHIFT },
    [RIGHT_OP]       = { NULL,              infix_binary,      PREC_SHIFT },
    ['=']            = { NULL,              infix_assignment,  PREC_ASSIGN },
    [ADD_ASSIGN]     = { NULL,              infix_assignment,  PREC_ASSIGN },
    [SUB_ASSIGN]     = { NULL,              infix_assignment,  PREC_ASSIGN },
    [MUL_ASSIGN]     = { NULL,              infix_assignment,  PREC_ASSIGN },
    [DIV_ASSIGN]     = { NULL,              infix_assignment,  PREC_ASSIGN },
    [AND_ASSIGN]     = { NULL,              infix_assignment,  PREC_ASSIGN },
    [OR_ASSIGN]      = { NULL,              infix_assignment,  PREC_ASSIGN },
    [XOR_ASSIGN]     = { NULL,              infix_assignment,  PREC_ASSIGN },
    [ALIGN_OP]       = { prefix_alignof,    NULL,              PREC_NONE },
    [GENERIC]        = { prefix_generic,    NULL,              PREC_NONE },
};

static const ParseRule* get_rule(int token_type) {
    if (token_type < 0 || token_type >= (int)(sizeof(rules)/sizeof(rules[0]))) {
        static const ParseRule empty = { NULL, NULL, PREC_NONE };
        return &empty;
    }
    return &rules[token_type];
}

static nu_ast_node_t* parse_expr_with_precedence(Precedence prec) {
    PrefixFn prefix = get_rule(p.current.token)->prefix;
    if (!prefix) {
        yyerror("Syntax Error: Expected expression");
        exit(EXIT_FAILURE);
    }
    nu_ast_node_t *left = prefix();
    while (prec <= get_rule(p.current.token)->precedence) {
        InfixFn infix = get_rule(p.current.token)->infix;
        if (!infix) break;
        left = infix(left);
    }
    return left;
}

nu_ast_node_t* parse_expression(void) {
    return parse_expr_with_precedence(PREC_COMMA);
}

static int is_type_specifier_start(void) {
    KeywordFlag flags = get_kw_flags();
    int tok = p.current.token;
    return (flags & (KW_TYPE_SPEC | KW_STORAGE | KW_QUALIFIER | KW_ALIGN)) ||
            tok == TYPE_NAME || (tok == IDENTIFIER && is_registered_type(p.current.text));
}

static nu_ast_node_t* last_parsed_struct_members = NULL;

static int parse_declaration_specifiers(void) {
    int is_typedef = 0;
    while (1) {
        KeywordFlag flags = get_kw_flags();
        int tok = p.current.token;

        if (tok == TYPEDEF || (p.current.text && strcmp(p.current.text, "typedef") == 0)) {
            is_typedef = 1;
            advance();
	} else if (tok == STRUCT || tok == UNION) {
            advance();
            char *struct_name = NULL;
            if (match(IDENTIFIER) || match(TYPE_NAME)) {
                struct_name = dup_string(p.current.text);
                advance();
            }
            if (match('{')) {
                advance();
                nu_ast_node_t *member_list = NULL;
                while (!match('}') && !match(TOKEN_EOF)) {
                    nu_ast_node_t *member = parse_declaration();
                    member_list = link_sibling(member_list, member);
                }
                consume('}', "Expected '}' after struct/union body");
                if (struct_name) {
                    register_struct(struct_name, member_list);
                } else {
                    last_parsed_struct_members = member_list;
                }
            }
        } else if (tok == ENUM) {
            advance();
            if (match(IDENTIFIER) || match(TYPE_NAME)) {
                advance();
            }
            if (match('{')) {
                advance();
                long current_val = 0;
                while (!match('}') && !match(TOKEN_EOF)) {
                    if (match(IDENTIFIER) || match(TYPE_NAME)) {
                        char *const_name = dup_string(p.current.text);
                        advance();
                        if (match('=')) {
                            advance();
                            if (match(CONSTANT)) {
                                current_val = p.current.val.int_val;
                                advance();
                            } else {
                                parse_initializer();
                            }
                        }
                        if (enum_const_map) {
                            nu_map_set(enum_const_map, const_name, (void*)(uintptr_t)(current_val + 1));
                        }
                        current_val++;
                    }
                    if (match(',')) advance();
                    else break;
                }
                consume('}', "Expected '}' after enum body");
            }
        } else if (is_type_specifier_start()) {
            advance();
        } else if (flags & KW_ALIGN) {
            advance();
            consume('(', "Expected '(' after alignment specifier");
            parse_declarator();
            consume(')', "Expected matching ')'");
	} else {
            break;
        }
    }
    return is_typedef;
}

static nu_ast_node_t* parse_declarator(void) {
    int ptr_depth = 0;
    while (match('*')) {
        ptr_depth++;
        advance();
        while (get_kw_flags() & KW_QUALIFIER) {
            advance();
        }
    }

    nu_ast_node_t *decl_node = NULL;
    if (match(IDENTIFIER) || match(TYPE_NAME)) {
        decl_node = nu_ast_new_node(g_ast, AST_IDENTIFIER);
        char *saved_name = dup_string(p.current.text);
        nu_ast_set_str(g_ast, decl_node, saved_name, strlen(saved_name));
        advance();
    } else if (match('(')) {
        advance();
        decl_node = parse_declarator();
        consume(')', "Expected matching ')'");
    }

    while (match('[') || match('(')) {
        int is_fn = match('(');
        advance();
        if (is_fn) {
            nu_ast_node_t *param_list = NULL;
            if (!match(')')) {
                while (!match(')') && !match(TOKEN_EOF)) {
                    if (match(IDENTIFIER) && strcmp(p.current.text, "void") == 0) {
                        advance();
                    } else if (is_type_specifier_start()) {
                        parse_declaration_specifiers();
                        nu_ast_node_t *param_decl = parse_declarator();
                        if (param_decl) {
                            param_list = link_sibling(param_list, param_decl);
                        }
                    } else {
                        advance();
                    }
                    if (match(',')) {
                        advance();
                    } else {
                        break;
                    }
                }
            }
            consume(')', "Expected matching ')'");
            if (decl_node) {
                decl_node = nu_ast_new_branch(g_ast, AST_FUNCTION_DECLARATOR, 2, decl_node, param_list);
            }
        } else {
            nu_ast_node_t *size_expr = NULL;
	    if (!match(']')) {
                size_expr = parse_expr_with_precedence(PREC_COMMA);
            } else {
                size_expr = nu_ast_new_node(g_ast, AST_INTEGER_LITERAL);
                nu_ast_set_int(size_expr, 0);
            }
            consume(']', "Expected matching ']'");
            if (decl_node) {
                decl_node = nu_ast_new_branch(g_ast, AST_ARRAY_REFERENCE, 2, decl_node, size_expr);
            }
        }
    }

    while (ptr_depth > 0) {
        decl_node = nu_ast_new_branch(g_ast, AST_UNARY_DEREFERENCE, 1, decl_node);
        ptr_depth--;
    }

    return decl_node;
}

static nu_ast_node_t* parse_static_assert(void) {
    if (!g_c11_enabled) {
        yyerror("_Static_assert is a C11-only feature.");
        exit(EXIT_FAILURE);
    }
    advance();
    consume('(', "Expected '(' after _Static_assert");
    nu_ast_node_t *expr = parse_expr_with_precedence(PREC_ASSIGN);
    consume(',', "Expected ',' after static assert expression");

    char string_val[1024];
    strncpy(string_val, p.current.text, sizeof(string_val) - 1);
    consume(STRING_LITERAL, "Expected string literal diagnostic message");
    consume(')', "Expected matching ')'");
    consume(';', "Expected ';' after static assert");

    nu_ast_node_t *msg_node = nu_ast_new_node(g_ast, AST_STRING_LITERAL);
    nu_ast_set_str(g_ast, msg_node, string_val + 1, strlen(string_val) - 2);
    return nu_ast_new_branch(g_ast, AST_STATIC_ASSERT, 2, expr, msg_node);
}

static int count_siblings(nu_ast_node_t *node) {
    int count = 0;
    while (node) {
        count++;
        node = node->next_sibling;
    }
    return count;
}

static nu_ast_node_t* parse_declaration(void) {
    if (g_c11_enabled && match(STATIC_ASSERT)) {
        return parse_static_assert();
    }

    int is_typedef = parse_declaration_specifiers();

    if (match(';')) {
        advance();
        return NULL;
    }

    nu_ast_node_t *decl_list = NULL;
    while (1) {
        nu_ast_node_t *decl = parse_declarator();

        if (is_typedef && decl) {
            const char *type_name = find_identifier_str(decl);
            if (type_name) {
                add_type(type_name);
                if (last_parsed_struct_members) {
                    register_struct(type_name, last_parsed_struct_members);
                    last_parsed_struct_members = NULL;
                }
            }
        }

	if (match('=')) {
	        advance();
	        nu_ast_node_t *init = parse_initializer();
		if (decl && decl->type == AST_ARRAY_REFERENCE) {
	            nu_ast_node_t *size_node = decl->first_child ? decl->first_child->next_sibling : NULL;
	            if (size_node && size_node->type == AST_INTEGER_LITERAL && ast_get_int(size_node) == 0) {
	                nu_ast_set_int(size_node, count_siblings(init));
	            }
	        }
	        decl = nu_ast_new_branch(g_ast, AST_ASSIGN_EXPR, 2, decl, init);
	}
        decl_list = link_sibling(decl_list, decl);
        if (match(',')) advance();
        else break;
    }
    consume(';', "Expected ';' after declaration");
    if (is_typedef) {
        return NULL;
    }
    nu_ast_node_t *node = nu_ast_new_node(g_ast, AST_DECLARATION);
    if (decl_list) nu_ast_add_child(node, decl_list);
    return node;
}

static nu_ast_node_t* parse_statement(void);

static nu_ast_node_t* parse_block_item(void) {
    int tok = p.current.token;
    if (is_type_specifier_start() || (g_c11_enabled && tok == STATIC_ASSERT)) {
        return parse_declaration();
    }
    return parse_statement();
}

static nu_ast_node_t* parse_compound_statement(void) {
    consume('{', "Expected '{'");
    if (match('}')) {
        advance();
        return nu_ast_new_node(g_ast, AST_COMPOUND_STATEMENT);
    }
    nu_ast_node_t *items = NULL;
    while (!match('}') && !match(TOKEN_EOF)) {
        items = link_sibling(items, parse_block_item());
    }
    consume('}', "Expected matching '}'");
    return nu_ast_new_branch(g_ast, AST_COMPOUND_STATEMENT, 1, items);
}

static nu_ast_node_t* parse_if_statement(void) {
    advance();
    consume('(', "Expected '(' after 'if'");
    nu_ast_node_t *cond = parse_expression();
    consume(')', "Expected matching ')'");
    nu_ast_node_t *then_branch = parse_statement();
    if (match(ELSE)) {
        advance();
        return nu_ast_new_branch(g_ast, AST_IF_STATEMENT, 3, cond, then_branch, parse_statement());
    }
    return nu_ast_new_branch(g_ast, AST_IF_STATEMENT, 2, cond, then_branch);
}

static nu_ast_node_t* parse_while_statement(void) {
    advance();
    consume('(', "Expected '(' after 'while'");
    nu_ast_node_t *cond = parse_expression();
    consume(')', "Expected matching ')'");
    return nu_ast_new_branch(g_ast, AST_WHILE_STATEMENT, 2, cond, parse_statement());
}

static nu_ast_node_t* parse_statement(void) {
    if (match('{')) return parse_compound_statement();
    if (match(IF)) return parse_if_statement();
    if (match(WHILE)) return parse_while_statement();
    if (match(FOR)) return parse_for_statement();
    if (match(RETURN)) {
        advance();
        if (match(';')) {
            advance();
            return nu_ast_new_node(g_ast, AST_RETURN_STATEMENT);
        }
        nu_ast_node_t *expr = parse_expression();
        consume(';', "Expected ';' after return");
        return nu_ast_new_branch(g_ast, AST_RETURN_STATEMENT, 1, expr);
    }
    if (match(ASM)) {
        advance();
        consume('(', "Expected '(' after asm");
        char asm_text[1024];
        strncpy(asm_text, p.current.text, sizeof(asm_text)-1);
        consume(STRING_LITERAL, "Expected string literal inside asm()");
        consume(')', "Expected matching ')'");
        consume(';', "Expected ';' after asm clause");
        nu_ast_node_t *node = nu_ast_new_node(g_ast, AST_INLINE_ASM);
        nu_ast_set_str(g_ast, node, asm_text + 1, strlen(asm_text) - 2);
        return node;
    }
    if (match(';')) {
        advance();
        return NULL;
    }
    nu_ast_node_t *expr = parse_expression();
    consume(';', "Expected ';' after expression statement");
    return expr;
}

nu_ast_node_t* parse_translation_unit(void) {
    init_parser();
    g_ast->root = NULL;
    while (!match(TOKEN_EOF)) {
        if (g_c11_enabled && match(STATIC_ASSERT)) {
            nu_ast_node_t *assert_node = parse_static_assert();
            if (!g_ast->root) {
                g_ast->root = nu_ast_new_branch(g_ast, AST_TRANSLATION_UNIT, 1, assert_node);
            } else {
                nu_ast_add_child(g_ast->root, assert_node);
            }
            continue;
        }
        int is_typedef = parse_declaration_specifiers();
        if (match(';')) {
            advance();
            continue;
        }
        nu_ast_node_t *decl = parse_declarator();

        if (is_typedef && decl) {
            const char *type_name = find_identifier_str(decl);
            if (type_name) {
                add_type(type_name);
                if (last_parsed_struct_members) {
                    register_struct(type_name, last_parsed_struct_members);
                    last_parsed_struct_members = NULL;
                }
            }
        }

        nu_ast_node_t *unit = NULL;
        if (match('{')) {
            unit = nu_ast_new_branch(g_ast, AST_FUNCTION_DEF, 2, decl, parse_compound_statement());

            const char *fname = find_identifier_str(decl);
            if (fname) {
                nu_ast_set_str(g_ast, unit, fname, strlen(fname));
            }
        } else {
            if (match('=')) {
                advance();
                nu_ast_node_t *init = parse_initializer();
		if (decl && decl->type == AST_ARRAY_REFERENCE) {
			nu_ast_node_t *size_node = decl->first_child ? decl->first_child->next_sibling : NULL;
	                if (size_node && size_node->type == AST_INTEGER_LITERAL && ast_get_int(size_node) == 0) {
		                nu_ast_set_int(size_node, count_siblings(init));
	                }
	        }
                decl = nu_ast_new_branch(g_ast, AST_ASSIGN_EXPR, 2, decl, init);
            }
            consume(';', "Expected ';' at declaration scope");
            if (!is_typedef) {
                unit = nu_ast_new_node(g_ast, AST_DECLARATION);
                if (decl) nu_ast_add_child(unit, decl);
            }
        }
        if (!g_ast->root) {
            g_ast->root = nu_ast_new_branch(g_ast, AST_TRANSLATION_UNIT, 1, unit);
        } else if (unit) {
            nu_ast_add_child(g_ast->root, unit);
        }
    }
    return g_ast->root;
}

int yyparse(void) {
    parse_translation_unit();
    return 0;
}
