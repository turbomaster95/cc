#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdint.h>

struct nu_ast_node;

#define IR_OP_LIST(X) \
    X(IR_FUNC_START)    \
    X(IR_FUNC_END)      \
    X(IR_RETURN)        \
    \
    X(IR_LOAD_INT)      \
    X(IR_LOAD_LOCAL)    \
    X(IR_STORE_LOCAL)   \
    X(IR_STORE_GLOBAL)  \
    X(IR_LOAD_GLOBAL)   \
    \
    X(IR_LEA_LOCAL)     \
    X(IR_LOAD_DEREF)    \
    X(IR_STORE_DEREF)   \
    \
    X(IR_PUSH_TEMP)     \
    X(IR_POP_TEMP)      \
    \
    X(IR_MEMBER_OFFSET) \
    X(IR_INDEX_OFFSET)  \
    \
    X(IR_ADD)           \
    X(IR_SUB)           \
    X(IR_MUL)           \
    X(IR_DIV)           \
    \
    X(IR_EQ)            \
    X(IR_NE)            \
    X(IR_LT)            \
    X(IR_GT)            \
    X(IR_LE)            \
    X(IR_GE)            \
    \
    X(IR_PARAM_DECL)   \
    X(IR_GLOBAL_DECL)  \
    X(IR_ARG_PUSH)     \
    X(IR_CALL)         \
    X(IR_LABEL)        \
    X(IR_JMP)          \
    X(IR_JMP_Z)        \
    X(IR_JMP_NZ)       \
    X(IR_CMP)          \
    X(IR_INLINE_ASM)   \
    X(IR_LOAD_STRING)

typedef enum {
#define X(name) name,
    IR_OP_LIST(X)
#undef X
} ir_op_t;

static inline const char* ir_op_name(ir_op_t op) {
    static const char* const names[] = {
#define X(name) #name,
        IR_OP_LIST(X)
#undef X
    };

    unsigned u = (unsigned)op;
    unsigned n = (unsigned)(sizeof(names) / sizeof(names[0]));
    return (u < n) ? names[u] : "(unknown)";
}


typedef struct {
    ir_op_t op;
    const char *label_name; // Re-used for function names, variables, and labels
    int64_t imm_val;        // Integer literals
    int32_t stack_offset;   // Abstract local frame offsets determined by walker
    int32_t param_index;    // Argument/parameter position (0, 1, 2...)
} ir_insn_t;

typedef struct {
    const char *target_name;
    void (*emit)(const ir_insn_t *insn);
} target_ops_t;

extern const target_ops_t *g_target;
void emit_ir(const ir_insn_t *insn);
void compile_ast(struct nu_ast_node *node);

#endif // CODEGEN_H
