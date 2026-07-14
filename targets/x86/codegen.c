#include <stdio.h>
#include <codegen.h>

static void x86_64_func_start(const char *name) {
    printf(".global %s\n", name);
    printf("%s:\n", name);
    printf("    pushq %%rbp\n");
    printf("    movq %%rsp, %%rbp\n");
}

static void x86_64_func_end(const char *name) {
    printf("# End of function %s\n\n", name);
}

static void x86_64_load_int(int64_t value) {
    printf("    movq $%ld, %%rax\n", (long)value);
}

static void x86_64_return(void) {
    printf("    movq %%rbp, %%rsp\n");
    printf("    popq %%rbp\n");
    printf("    ret\n");
}

static void x86_64_label(const char *name) {
    printf("%s:\n", name);
}

static const target_ops_t x86_64_target = {
    .target_name    = "x86_64",
    .gen_func_start = x86_64_func_start,
    .gen_func_end   = x86_64_func_end,
    .gen_load_int   = x86_64_load_int,
    .gen_return     = x86_64_return,
    .gen_label      = x86_64_label
};

const target_ops_t *g_target = &x86_64_target; 

void emit_ir(const ir_insn_t *insn) {
    if (!g_target) return;

    switch (insn->op) {
        case IR_FUNC_START:
            if (g_target->gen_func_start) g_target->gen_func_start(insn->label_name);
            break;
        case IR_FUNC_END:
            if (g_target->gen_func_end) g_target->gen_func_end(insn->label_name);
            break;
        case IR_LOAD_INT:
            if (g_target->gen_load_int) g_target->gen_load_int(insn->imm_val);
            break;
        case IR_RETURN:
            if (g_target->gen_return) g_target->gen_return();
            break;
        case IR_LABEL:
            if (g_target->gen_label) g_target->gen_label(insn->label_name);
            break;
    }
}
