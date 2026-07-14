#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdint.h>

struct nu_ast_node;

typedef enum {
    IR_FUNC_START,   // Starts a function block
    IR_FUNC_END,     // Ends a function block
    IR_RETURN,

    // Symbol Operations
    IR_LOAD_INT,       // Load immediate value into virtual accumulator
    IR_LOAD_LOCAL,     // Load variable value from symbolic offset
    IR_STORE_LOCAL,    // Store virtual accumulator into symbolic offset
    
    // Pointer & Memory Operations
    IR_LEA_LOCAL,      // Load absolute address of a local variable
    IR_LOAD_DEREF,     // Read from address currently in accumulator
    IR_STORE_DEREF,    // Store value into address currently in accumulator

    // Expression Processing
    IR_PUSH_TEMP,      // Push virtual accumulator onto a virtual evaluation stack
    IR_POP_TEMP,       // Pop virtual stack into an auxiliary virtual register

    // Mathematical operations
    IR_ADD, IR_SUB, IR_MUL, IR_DIV,
    IR_EQ, IR_NE, IR_LT, IR_GT, IR_LE, IR_GE,  
    
    IR_PARAM_DECL,     // Mid-end declares an incoming parameter: label_name at stack_offset
    IR_ARG_PUSH,       // Push an argument expression for an upcoming function call
    IR_CALL,           // Execute call to target label string
    IR_LABEL,          // Marker for jump targets
    IR_JMP,            // Unconditional jump
    IR_JMP_Z,          // Jump if zero
    IR_JMP_NZ,         // Jump if not zero
    IR_CMP,            // Compare top two values on stack
    IR_INLINE_ASM,
    IR_LOAD_STRING,    // Load string literal address
} ir_op_t;

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
