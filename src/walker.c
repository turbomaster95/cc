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

typedef struct Symbol {
    char name[32];
    int32_t offset;
    struct Symbol *next;
} Symbol;

static Symbol *g_symbols = NULL;

static int32_t lookup_var(const char *name) {
    for (Symbol *s = g_symbols; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s->offset;
    return 0; // Not found
}

static int g_label_count = 0;

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
	    const char *name = find_identifier_str(decl_node);
    
	    if (!name) break; // Safety net

	    int32_t offset = allocate_local(name);

	    Symbol *s = malloc(sizeof(Symbol));
	    strcpy(s->name, name); 
	    s->offset = offset; 
	    s->next = g_symbols; 
	    g_symbols = s;

	    // If it's an initialized declaration (e.g., int a = 5), decl_node is AST_ASSIGN_EXPR.
	    // first_child is the LHS (the identifier), last_child is the RHS (the '5').
	    if (decl_node->type == AST_ASSIGN_EXPR && 
	        decl_node->first_child && 
	        decl_node->last_child && 
	        decl_node->first_child != decl_node->last_child) {
        
	        compile_ast(decl_node->last_child); // Evaluate the RHS
	        ir_insn_t store = { .op = IR_STORE_LOCAL, .stack_offset = offset };
	        emit_ir(&store);
	    }
	    break;
        }
        case AST_ASSIGN_EXPR: {
            nu_ast_node_t *lhs = node->first_child;
            nu_ast_node_t *rhs = node->last_child;

            if (lhs && rhs && lhs->type == AST_IDENTIFIER && lhs->val.str) {
                compile_ast(rhs);
                
                int32_t offset = lookup_var(lhs->val.str);
                
                ir_insn_t store = { .op = IR_STORE_LOCAL, .stack_offset = offset };
                emit_ir(&store);
            } else {
                // Fallback for complex assignments
                for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                    compile_ast(c);
                }
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

        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_LT:
        case AST_BINARY_GT:
        case AST_BINARY_LE:
        case AST_BINARY_GE: {
            compile_ast(node->first_child);               // Left
            ir_insn_t push = { .op = IR_PUSH_TEMP }; emit_ir(&push);
            compile_ast(node->first_child->next_sibling); // Right
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
	        ir_insn_t load = { .op = IR_LOAD_LOCAL, .stack_offset = lookup_var(node->val.str) };
	        emit_ir(&load);
	        break;
        }

	case AST_BINARY_ADD: 
        case AST_BINARY_SUB: 
	case AST_BINARY_MUL: {
		compile_ast(node->first_child);               // Left
	        ir_insn_t push = { .op = IR_PUSH_TEMP }; emit_ir(&push);
	        compile_ast(node->first_child->next_sibling); // Right
	        ir_insn_t pop = { .op = IR_POP_TEMP }; emit_ir(&pop);

		ir_op_t opcode = IR_ADD; // dummy init
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
           
           compile_ast(node->first_child); // Condition
           ir_insn_t cmp = { .op = IR_CMP, .imm_val = 0 }; emit_ir(&cmp); 
           ir_insn_t jmp_else = { .op = IR_JMP_Z, .imm_val = label_else }; emit_ir(&jmp_else);           
           compile_ast(node->first_child->next_sibling); // IF Body
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

        default:
            for (nu_ast_node_t *c = node->first_child; c; c = c->next_sibling) {
                compile_ast(c);
            }
            break;
    }
}
