#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nu.h>
#include <compiler.h>
#include <codegen.h>

static int32_t g_frame_offset = 0;

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

void compile_ast(nu_ast_node_t *node) {
    if (!node) return;

    switch (node->type) {
        case AST_TRANSLATION_UNIT: {
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
        }

        case AST_FUNCTION_DEF: {
            g_frame_offset = 0;
            
            const char *func_name = find_identifier_str(node);
            if (!func_name) func_name = "main";

            ir_insn_t start = { .op = IR_FUNC_START, .label_name = func_name };
            emit_ir(&start);

            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                if (c->type == AST_COMPOUND_STATEMENT) {
                    compile_ast(c);
                }
            }

            ir_insn_t end = { .op = IR_FUNC_END, .label_name = func_name };
            emit_ir(&end);
            break;
        }

        case AST_COMPOUND_STATEMENT: {
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
        }

	case AST_RETURN_STATEMENT: {
            nu_ast_node_t *expr = node->first_child;
            int64_t ret_val = 0;

            if (expr && expr->type == AST_INTEGER_LITERAL) {
                ret_val = expr->val.i64;
            }

            ir_insn_t load_imm = {
                .op = IR_LOAD_INT, 
                .imm_val = ret_val 
            };
            emit_ir(&load_imm);

            ir_insn_t ret_insn = { .op = IR_RETURN };
            emit_ir(&ret_insn);
            break;
        }

        default:
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
    }
}
