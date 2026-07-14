#ifndef CODEGEN_H
#define CODEGEN_H

#include "compiler.h"

typedef enum {
    IR_LABEL,
    IR_FUNC_START,
    IR_FUNC_END,
    IR_LOAD_INT,
    IR_RETURN,
} ir_op_t;

typedef struct {
    ir_op_t op;
    const char *label_name;
    int64_t imm_val;
} ir_insn_t;

typedef struct {
    const char *target_name;
    void (*gen_func_start)(const char *name);
    void (*gen_func_end)(const char *name);
    void (*gen_load_int)(int64_t value);
    void (*gen_return)(void);
    void (*gen_label)(const char *name);
} target_ops_t;

void emit_ir(const ir_insn_t *insn);
void compile_ast(nu_ast_node_t *node);

extern const target_ops_t *g_target;

#endif // CODEGEN_H
