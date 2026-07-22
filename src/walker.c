#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nu.h>
#include <compiler.h>
#include <codegen.h>

static int32_t g_frame_offset = 0;
extern nu_mm_t *g_mm;

static int32_t allocate_local(const char *name) {
    (void)name;
    g_frame_offset -= 8;
    return g_frame_offset;
}

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

typedef enum { SCOPE_LOCAL, SCOPE_GLOBAL } Scope;
static int g_in_function = 0;

typedef struct Symbol {
    char name[32];
    int32_t offset;
    int32_t size;
    Scope scope;
    struct Symbol *next;
} Symbol;

static Symbol *g_symbols = NULL;
static Symbol *g_globals = NULL;

typedef struct StructField {
    char name[32];
    int32_t offset;
    int32_t size;
    struct StructField *next;
} StructField;

typedef struct StructLayout {
    char name[32];
    StructField *fields;
    int32_t size;
    struct StructLayout *next;
} StructLayout;

static StructLayout *g_structs = NULL;

static int32_t type_name_to_size(const char *type_str) {
    if (!type_str) return 4;
    if (strcmp(type_str, "char") == 0) return 1;
    if (strcmp(type_str, "short") == 0) return 2;
    if (strcmp(type_str, "int") == 0) return 4;
    if (strcmp(type_str, "long") == 0) return 8;
    if (strcmp(type_str, "float") == 0) return 4;
    if (strcmp(type_str, "double") == 0) return 8;

    for (StructLayout *sl = g_structs; sl; sl = sl->next) {
        if (strcmp(sl->name, type_str) == 0) {
            return sl->size;
        }
    }
    return 4;
}

static int32_t get_field_offset(const char *field_name) {
    if (!field_name) return 0;
    for (StructLayout *sl = g_structs; sl; sl = sl->next) {
        for (StructField *sf = sl->fields; sf; sf = sf->next) {
            if (strcmp(sf->name, field_name) == 0) {
                return sf->offset;
            }
        }
    }
    return 0;
}

void register_struct(const char *name, nu_ast_node_t *member_list) {
    if (!name) return;
    StructLayout *sl = nu_alloc(g_mm, sizeof(StructLayout));
    strcpy(sl->name, name);
    sl->fields = NULL;
    int32_t current_offset = 0;
    for (nu_ast_node_t *m = member_list; m; m = m->next_sibling) {
        const char *fname = find_identifier_str(m);
        const char *ftype = find_identifier_str(m->first_child);
        int32_t fsize = type_name_to_size(ftype);

        if (fname) {
            StructField *sf = nu_alloc(g_mm, sizeof(StructField));
            strcpy(sf->name, fname);
            sf->offset = current_offset;
            sf->size = fsize;
            sf->next = sl->fields;
            sl->fields = sf;
            current_offset += fsize;
        }
    }
    sl->size = current_offset;
    sl->next = g_structs;
    g_structs = sl;
}

static Symbol* find_symbol(const char *name) {
    if (!name) return NULL;
    for (Symbol *s = g_symbols; s; s = s->next)
        if (s->name && strcmp(s->name, name) == 0) return s;
    for (Symbol *s = g_globals; s; s = s->next)
        if (s->name && strcmp(s->name, name) == 0) return s;
    return NULL;
}

static int32_t lookup_var(const char *name) {
    if (!name) return 0;
    for (Symbol *s = g_symbols; s; s = s->next)
        if (s->name && strcmp(s->name, name) == 0) return s->offset;

    for (Symbol *s = g_globals; s; s = s->next)
        if (s->name && strcmp(s->name, name) == 0) return s->offset;

    return 0; // Not found
}

static int32_t get_expression_size(nu_ast_node_t *node) {
    if (!node) return 4;

    switch (node->type) {
        case AST_IDENTIFIER: {
            Symbol *s = find_symbol(node->val.str);
            if (s && s->size > 0) return s->size;
            return 4;
        }

        case AST_MEMBER_ACCESS:
        case AST_MEMBER_DEREF: {
            nu_ast_node_t *member = node->last_child;
            const char *mname = (member && member->type == AST_IDENTIFIER) ? member->val.str : NULL;
            if (mname) {
                for (StructLayout *sl = g_structs; sl; sl = sl->next) {
                    for (StructField *sf = sl->fields; sf; sf = sf->next) {
                        if (strcmp(sf->name, mname) == 0) {
                            return sf->size > 0 ? sf->size : 4;
                        }
                    }
                }
            }
            return 4;
        }

        case AST_UNARY_DEREFERENCE: {
            return 4;
        }

        case AST_ARRAY_REFERENCE: {
            return 4;
        }

        case AST_INTEGER_LITERAL:
            return 4;

        default:
            return 4;
    }
}

static void dump_ast_asm(nu_ast_node_t *node, int depth) {
    if (!node) return;
    char buf[256];
    char indent[32] = {0};
    for (int i = 0; i < depth && i < 15; i++) strcat(indent, "  ");

    snprintf(buf, sizeof(buf), "# %s type=%d", indent, node->type);

    if (node->type == AST_IDENTIFIER || node->type == AST_STRING_LITERAL) {
        if (node->val.str) snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " str='%s'", node->val.str);
    } else if (node->type == AST_INTEGER_LITERAL) {
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " int=%ld", (long)node->val.i64);
    }

    char *dyn_str = nu_alloc(g_mm, strlen(buf) + 1);
    strcpy(dyn_str, buf);
    ir_insn_t insn = { .op = IR_INLINE_ASM, .label_name = dyn_str };
    emit_ir(&insn);

    for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
        dump_ast_asm(c, depth + 1);
    }
}

static void extract_params(nu_ast_node_t *node, const char *func_name, int *param_idx) {
    if (!node) return;
    if (node->type == AST_COMPOUND_STATEMENT) return; // Do not enter the function body

    if (node->type == AST_IDENTIFIER && node->val.str) {
        const char *name = node->val.str;

        if (func_name && strcmp(name, func_name) != 0 &&
            strcmp(name, "int") != 0 &&
            strcmp(name, "char") != 0 &&
            strcmp(name, "void") != 0 &&
            strcmp(name, "long") != 0 &&
            strcmp(name, "short") != 0 &&
            strcmp(name, "float") != 0 &&
            strcmp(name, "double") != 0 &&
            strcmp(name, "struct") != 0) {

            if (lookup_var(name) == 0) {
                int32_t offset = allocate_local(name);
                Symbol *s = nu_alloc(g_mm, sizeof(Symbol));
                strcpy(s->name, name);
                s->offset = offset;
                s->scope = SCOPE_LOCAL;
                s->next = g_symbols;
                g_symbols = s;

                ir_insn_t param_dec = {
                    .op = IR_PARAM_DECL,
                    .param_index = (*param_idx)++,
                    .stack_offset = offset,
                    .label_name = name
                };
                emit_ir(&param_dec);
            }
        }
    }

    for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
        extract_params(c, func_name, param_idx);
    }
}

static int g_label_count = 0;
static int g_implicit_param_idx = 0;

void compile_ast(nu_ast_node_t *node);

static void compile_lvalue_address(nu_ast_node_t *node) {
    if (!node) return;

    switch (node->type) {
        case AST_IDENTIFIER: {
            Symbol *s = find_symbol(node->val.str);
            if (s) {
                ir_insn_t lea = {
                    .op = IR_LEA_LOCAL,
                    .stack_offset = (s->scope == SCOPE_LOCAL) ? s->offset : 0,
                    .label_name = (s->scope == SCOPE_GLOBAL) ? s->name : NULL
                };
                emit_ir(&lea);
            }
            break;
        }

	case AST_MEMBER_ACCESS: {
            compile_lvalue_address(node->first_child);
            nu_ast_node_t *member = node->last_child;
            const char *mname = (member && member->type == AST_IDENTIFIER) ? member->val.str : "unknown";
            int32_t offset = get_field_offset(mname);
            if (offset != 0) {
                ir_insn_t push = { .op = IR_PUSH_TEMP }; emit_ir(&push);
                ir_insn_t load_imm = { .op = IR_LOAD_INT, .imm_val = offset }; emit_ir(&load_imm);
                ir_insn_t pop = { .op = IR_POP_TEMP }; emit_ir(&pop);
                ir_insn_t add = { .op = IR_ADD }; emit_ir(&add);
            }
            break;
        }

        case AST_MEMBER_DEREF: {
            compile_ast(node->first_child);
            nu_ast_node_t *member = node->last_child;
            const char *mname = (member && member->type == AST_IDENTIFIER) ? member->val.str : "unknown";
            int32_t offset = get_field_offset(mname);
            if (offset != 0) {
                ir_insn_t push = { .op = IR_PUSH_TEMP }; emit_ir(&push);
                ir_insn_t load_imm = { .op = IR_LOAD_INT, .imm_val = offset }; emit_ir(&load_imm);
                ir_insn_t pop = { .op = IR_POP_TEMP }; emit_ir(&pop);
                ir_insn_t add = { .op = IR_ADD }; emit_ir(&add);
            }
            break;
        }

        case AST_UNARY_DEREFERENCE: {
            compile_ast(node->first_child);
            break;
        }
        case AST_ARRAY_REFERENCE: {
            compile_ast(node->first_child);
            ir_insn_t push = { .op = IR_PUSH_TEMP }; emit_ir(&push);
            compile_ast(node->last_child);
            ir_insn_t pop = { .op = IR_POP_TEMP }; emit_ir(&pop);
            break;
        }
        default:
            compile_ast(node);
            break;
    }
}

void compile_ast(nu_ast_node_t *node) {
    if (!node) return;

    switch (node->type) {
        case AST_TRANSLATION_UNIT: {
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
        }

        case AST_INTEGER_LITERAL: {
            ir_insn_t load_imm = { .op = IR_LOAD_INT, .imm_val = node->val.i64 };
            emit_ir(&load_imm);
            break;
        }

        case AST_DECLARATION: {
            if (!node->first_child) break;
            nu_ast_node_t *decl_node = node->first_child;
	    const char *type_str = find_identifier_str(decl_node->first_child);
	    int32_t var_size = type_name_to_size(type_str);

	    if (decl_node->first_child) {
                nu_ast_node_t *spec = decl_node->first_child;
                const char *struct_name = find_identifier_str(decl_node);
                if (struct_name && spec->first_child) {
                    register_struct(struct_name, spec->first_child);
                }
            }

            if (decl_node->type == AST_FUNCTION_DECLARATOR) break;

            const char *name = find_identifier_str(decl_node);
            if (!name) break;

            if (!g_in_function) { // GLOBAL DECLARATION
                Symbol *s = nu_alloc(g_mm, sizeof(Symbol));
                strcpy(s->name, name); s->scope = SCOPE_GLOBAL;
                s->next = g_globals; g_globals = s;
                ir_insn_t decl = { .op = IR_GLOBAL_DECL, .label_name = name };
                emit_ir(&decl);
            } else { // LOCAL DECLARATION
                int32_t offset = allocate_local(name);
                Symbol *s = nu_alloc(g_mm, sizeof(Symbol));
                strcpy(s->name, name); s->offset = offset; s->scope = SCOPE_LOCAL;
		s->size = var_size;
                s->next = g_symbols; g_symbols = s;
                if (decl_node->type == AST_ASSIGN_EXPR) {
                    compile_ast(decl_node->last_child);
                    ir_insn_t store = { .op = IR_STORE_LOCAL, .stack_offset = offset };
                    emit_ir(&store);
                }
            }
            break;
        }

	case AST_ASSIGN_EXPR: {
            nu_ast_node_t *lhs = node->first_child;
            nu_ast_node_t *rhs = node->last_child;

            if (lhs && lhs->type == AST_IDENTIFIER) {
                const char *lhs_name = lhs->val.str;
                Symbol *s = lhs_name ? find_symbol(lhs_name) : NULL;
                compile_ast(rhs);
                if (s && s->scope == SCOPE_GLOBAL) {
                    ir_insn_t store = { .op = IR_STORE_GLOBAL, .label_name = (char*)lhs_name };
                    emit_ir(&store);
                } else if (s) {
                    ir_insn_t store = { .op = IR_STORE_LOCAL, .stack_offset = s->offset };
                    emit_ir(&store);
                } else {
                    ir_insn_t store = { .op = IR_STORE_LOCAL, .stack_offset = 0 };
                    emit_ir(&store);
                }
            } else {
                compile_lvalue_address(lhs);
                ir_insn_t push = { .op = IR_PUSH_TEMP };
                emit_ir(&push);
                compile_ast(rhs);
                ir_insn_t pop = { .op = IR_POP_TEMP };
                emit_ir(&pop);
                int32_t size = get_expression_size(lhs);
                ir_insn_t store_ind = { .op = IR_STORE_DEREF, .imm_val = size };
                emit_ir(&store_ind);
            }
            break;
        }

        case AST_FUNCTION_DEF: {
            g_in_function = 1;
            g_frame_offset = 0;
            g_symbols = NULL;
            g_implicit_param_idx = 0;

            const char *func_name = node->val.str;
            if (!func_name) func_name = find_identifier_str(node);
            if (!func_name) func_name = "main";

            ir_insn_t start = { .op = IR_FUNC_START, .label_name = func_name };
            emit_ir(&start);

            int param_idx = 0;
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                if (c->type != AST_COMPOUND_STATEMENT) {
                    extract_params(c, func_name, &param_idx);
                }
            }

            g_implicit_param_idx = param_idx;

            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                if (c->type == AST_COMPOUND_STATEMENT) {
                    compile_ast(c);
                }
            }

            ir_insn_t end = { .op = IR_FUNC_END, .label_name = func_name };
            emit_ir(&end);
            g_in_function = 0;
            break;
        }

        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_LT:
        case AST_BINARY_GT:
        case AST_BINARY_LE:
        case AST_BINARY_GE: {
            compile_ast(node->first_child);
            ir_insn_t push = { .op = IR_PUSH_TEMP }; emit_ir(&push);
            compile_ast(node->first_child->next_sibling);
            ir_insn_t pop = { .op = IR_POP_TEMP }; emit_ir(&pop);

            ir_op_t opcode = IR_EQ;
            switch(node->type) {
                case AST_BINARY_EQ: opcode = IR_EQ; break;
                case AST_BINARY_NE: opcode = IR_NE; break;
                case AST_BINARY_LT: opcode = IR_LT; break;
                case AST_BINARY_GT: opcode = IR_GT; break;
                case AST_BINARY_LE: opcode = IR_LE; break;
                case AST_BINARY_GE: opcode = IR_GE; break;
            }
            ir_insn_t op = { .op = opcode };
            emit_ir(&op);
            break;
        }

        case AST_COMPOUND_STATEMENT: {
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
        }

        case AST_WHILE_STATEMENT: {
            int label_start = g_label_count++;
            int label_end = g_label_count++;

            ir_insn_t lbl_start = { .op = IR_LABEL, .imm_val = label_start };
            emit_ir(&lbl_start);

            compile_ast(node->first_child);

            ir_insn_t cmp = { .op = IR_CMP, .imm_val = 0 };
            emit_ir(&cmp);

            ir_insn_t jmp_end = { .op = IR_JMP_Z, .imm_val = label_end };
            emit_ir(&jmp_end);

            compile_ast(node->first_child->next_sibling);

            ir_insn_t jmp_start = { .op = IR_JMP, .imm_val = label_start };
            emit_ir(&jmp_start);

            ir_insn_t lbl_end = { .op = IR_LABEL, .imm_val = label_end };
            emit_ir(&lbl_end);
            break;
        }

        case AST_RETURN_STATEMENT: {
            nu_ast_node_t *expr = node->first_child;

            if (expr) {
                if (expr->type == AST_INTEGER_LITERAL) {
                    ir_insn_t load_imm = { .op = IR_LOAD_INT, .imm_val = expr->val.i64 };
                    emit_ir(&load_imm);
                } else {
                    compile_ast(expr);
                }
            }

            ir_insn_t ret_insn = { .op = IR_RETURN };
            emit_ir(&ret_insn);
            break;
        }

        case AST_IDENTIFIER: {
            if (!node->val.str) break;
            Symbol *s = find_symbol(node->val.str);
            if (s && s->scope == SCOPE_GLOBAL) {
                ir_insn_t load = { .op = IR_LOAD_GLOBAL, .label_name = node->val.str };
                emit_ir(&load);
            } else if (s) {
                ir_insn_t load = { .op = IR_LOAD_LOCAL, .stack_offset = s->offset };
                emit_ir(&load);
            } else {
                ir_insn_t load = { .op = IR_LOAD_INT, .imm_val = 0 };
                emit_ir(&load);
            }
            break;
        }

        case AST_UNARY_POST_INC:
        case AST_UNARY_POST_DEC:
        case AST_UNARY_PRE_INC:
        case AST_UNARY_PRE_DEC:
        case AST_UNARY_PLUS:
        case AST_UNARY_MINUS:
        case AST_UNARY_BIT_NOT:
        case AST_UNARY_LOGIC_NOT:
        case AST_COMPOUND_LITERAL: {
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
        }

	case AST_UNARY_ADDRESS: {
            compile_lvalue_address(node->first_child);
            break;
        }

	case AST_MEMBER_ACCESS: {
            int32_t size = get_expression_size(node);
            compile_lvalue_address(node);
            ir_insn_t load_ind = { .op = IR_LOAD_DEREF, .imm_val = size };
            emit_ir(&load_ind);
            break;
        }

        case AST_MEMBER_DEREF: {
            compile_lvalue_address(node);
            ir_insn_t load_ind = { .op = IR_LOAD_DEREF };
            emit_ir(&load_ind);
            break;
        }

        case AST_UNARY_DEREFERENCE: {
            compile_lvalue_address(node);
            ir_insn_t load_ind = { .op = IR_LOAD_DEREF };
            emit_ir(&load_ind);
            break;
        }

        case AST_ARRAY_REFERENCE: {
            compile_lvalue_address(node);
            ir_insn_t load_ind = { .op = IR_LOAD_DEREF };
            emit_ir(&load_ind);
            break;
        }

        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL: {
            compile_ast(node->first_child);
            ir_insn_t push = { .op = IR_PUSH_TEMP }; emit_ir(&push);
            compile_ast(node->first_child->next_sibling);
            ir_insn_t pop = { .op = IR_POP_TEMP }; emit_ir(&pop);

            ir_op_t opcode = IR_ADD;
            if (node->type == AST_BINARY_ADD) opcode = IR_ADD;
            else if (node->type == AST_BINARY_SUB) opcode = IR_SUB;
            else if (node->type == AST_BINARY_MUL) opcode = IR_MUL;

            ir_insn_t op = { .op = opcode };
            emit_ir(&op);
            break;
        }

        case AST_IF_STATEMENT: {
            int label_else = g_label_count++;
            int label_end = g_label_count++;

            compile_ast(node->first_child);
            ir_insn_t cmp = { .op = IR_CMP, .imm_val = 0 }; emit_ir(&cmp);
            ir_insn_t jmp_else = { .op = IR_JMP_Z, .imm_val = label_else }; emit_ir(&jmp_else);
            compile_ast(node->first_child->next_sibling);
            ir_insn_t jmp_end = { .op = IR_JMP, .imm_val = label_end }; emit_ir(&jmp_end);

            ir_insn_t lbl_else = { .op = IR_LABEL, .imm_val = label_else }; emit_ir(&lbl_else);
            if (node->first_child->next_sibling->next_sibling) {
                compile_ast(node->first_child->next_sibling->next_sibling);
            }
            ir_insn_t lbl_end = { .op = IR_LABEL, .imm_val = label_end }; emit_ir(&lbl_end);
            break;
        }

        case AST_INLINE_ASM: {
            ir_insn_t asm_insn = { .op = IR_INLINE_ASM, .label_name = node->val.str };
            emit_ir(&asm_insn);
            break;
        }

        case AST_STRING_LITERAL: {
            ir_insn_t load_str = { .op = IR_LOAD_STRING, .label_name = node->val.str };
            emit_ir(&load_str);
            break;
        }

        case AST_FUNCTION_CALL: {
            nu_ast_node_t *func_node = node->first_child;
            const char *func_name = find_identifier_str(func_node);
            if (!func_name) func_name = "unknown";

            int arg_index = 0;
            for (nu_ast_node_t *arg = func_node->next_sibling; arg; arg = arg->next_sibling) {
                compile_ast(arg);

                ir_insn_t push_arg = { .op = IR_ARG_PUSH, .param_index = arg_index++ };
                emit_ir(&push_arg);
            }

            ir_insn_t call_insn = { .op = IR_CALL, .label_name = func_name };
            emit_ir(&call_insn);
            break;
        }

	case AST_FOR_STATEMENT: {
	    int label_start = g_label_count++;
	    int label_end = g_label_count++;
	    compile_ast(node->first_child);
	    ir_insn_t lbl_start = { .op = IR_LABEL, .imm_val = label_start };
	    emit_ir(&lbl_start);
	    compile_ast(node->first_child->next_sibling);
	    ir_insn_t cmp = { .op = IR_CMP, .imm_val = 0 };
	    emit_ir(&cmp);
	    ir_insn_t jmp_end = { .op = IR_JMP_Z, .imm_val = label_end };
	    emit_ir(&jmp_end);
	    compile_ast(node->first_child->next_sibling->next_sibling->next_sibling);
	    ir_insn_t lbl_inc = { .op = IR_LABEL, .imm_val = g_label_count++ };
	    emit_ir(&lbl_inc);
	    compile_ast(node->first_child->next_sibling->next_sibling);
	    ir_insn_t jmp_start = { .op = IR_JMP, .imm_val = label_start };
	    emit_ir(&jmp_start);
	    ir_insn_t lbl_end = { .op = IR_LABEL, .imm_val = label_end };
	    emit_ir(&lbl_end);
	    break;
	}

        default:
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
    }
}

