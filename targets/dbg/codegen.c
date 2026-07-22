#include <stdio.h>
#include <codegen.h>

#define INDENT_STEP 2

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        for (int j = 0; j < INDENT_STEP; j++) putchar(' ');
    }
}

static int g_depth = 0;

void parser_init_hook(void) {
	g_depth = 0;
}

static void dbg_emit(const ir_insn_t *insn) {
    if (insn->op == IR_FUNC_START) {
        print_indent(g_depth);
        printf("Function %s\n", insn->label_name ? insn->label_name : "(null)");
        g_depth++;
        return;
    }

    if (insn->op == IR_FUNC_END) {
        g_depth = (g_depth > 0) ? (g_depth - 1) : 0;
        print_indent(g_depth);
        printf("End Function %s\n", insn->label_name ? insn->label_name : "(null)");
        return;
    }

    print_indent(g_depth);
    printf("- %s", ir_op_name(insn->op));

    if (insn->label_name &&
        (insn->op == IR_CALL ||
         insn->op == IR_LABEL ||
         insn->op == IR_FUNC_START ||
         insn->op == IR_GLOBAL_DECL ||
         insn->op == IR_PARAM_DECL ||
         insn->op == IR_INLINE_ASM ||
         insn->op == IR_LOAD_STRING)) {
        printf(" label=%s", insn->label_name);
    }

    if (insn->op == IR_LOAD_INT ||
        insn->op == IR_JMP || insn->op == IR_JMP_Z || insn->op == IR_JMP_NZ ||
        insn->op == IR_CMP || insn->op == IR_MEMBER_OFFSET || insn->op == IR_INDEX_OFFSET ||
        insn->op == IR_PARAM_DECL) {
        printf(" imm=%ld", (long)insn->imm_val);
    }

    if (insn->op == IR_LOAD_LOCAL ||
        insn->op == IR_STORE_LOCAL ||
        insn->op == IR_LEA_LOCAL ||
        insn->op == IR_PARAM_DECL) {
        printf(" stck=%d", insn->stack_offset);
    }

    if (insn->op == IR_PARAM_DECL || insn->op == IR_ARG_PUSH) {
        printf(" param=%d", insn->param_index);
    }

    putchar('\n');
}

const target_ops_t dbg_target = { .target_name = "debug", .emit = dbg_emit };
const target_ops_t *g_target = &dbg_target;

