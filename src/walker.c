#include <stdio.h>
#include <stdlib.h>
#include <compiler.h>
#include <codegen.h>

#define MAX_RECURSION_DEPTH 256

static void compile_ast_safe(nu_ast_node_t *node, int depth) {
    if (!node) return;

    if (depth > MAX_RECURSION_DEPTH) {
        fprintf(stderr, "Backend Error: Maximum AST depth exceeded (potential loop detected).\n");
        return;
    }

    if (node->next_sibling == node) {
        fprintf(stderr, "Backend Error: Cyclic node reference detected on node type %d!\n", node->type);
        return;
    }

    switch (node->type) {
        case AST_FUNCTION_DEF: {
            ir_insn_t start = { .op = IR_FUNC_START, .label_name = "main" };
            emit_ir(&start);

            if (node->first_child) {
                compile_ast_safe(node->first_child, depth + 1);
            }

            ir_insn_t end = { .op = IR_FUNC_END, .label_name = "main" };
            emit_ir(&end);
            break;
        }

        case AST_RETURN_STATEMENT: {
            if (node->first_child) {
                compile_ast_safe(node->first_child, depth + 1);
            }
            ir_insn_t ret = { .op = IR_RETURN };
            emit_ir(&ret);
            break;
        }

        case AST_INTEGER_LITERAL: {
            ir_insn_t load = { .op = IR_LOAD_INT, .imm_val = node->val.i64 };
            emit_ir(&load);
            break;
        }

        case AST_STRUCT_SPECIFIER:
        case AST_STRUCT_DECLARATION:
            // Skip processing children of pure type definitions to avoid aggregate parsing bugs
            break;

        default:
            if (node->first_child) {
                compile_ast_safe(node->first_child, depth + 1);
            }
            break;
    }

    if (node->next_sibling && node->next_sibling != node) {
        compile_ast_safe(node->next_sibling, depth);
    }
}

void compile_ast(nu_ast_node_t *node) {
    compile_ast_safe(node, 0);
}
