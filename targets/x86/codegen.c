#include <stdio.h>
#include <codegen.h>

static const char* x86_abi_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static void x86_emit(const ir_insn_t *insn) {
    switch (insn->op) {
        case IR_FUNC_START:
            printf(".global %s\n%s:\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n\tsubq $256, %%rsp\n", insn->label_name, insn->label_name);
            break;
        case IR_PARAM_DECL:
            if (insn->param_index < 6) {
                printf("\tmovq %%%s, %d(%%rbp) # Bind %s\n", x86_abi_regs[insn->param_index], insn->stack_offset, insn->label_name);
            }
            break;
        case IR_LOAD_INT:   printf("\tmovq $%ld, %%rax\n", (long)insn->imm_val); break;
        case IR_LOAD_LOCAL: printf("\tmovq %d(%%rbp), %%rax\n", insn->stack_offset); break;
        case IR_PUSH_TEMP:  printf("\tpushq %%rax\n"); break;
        case IR_POP_TEMP:   printf("\tpopq %%rcx\n"); break;
        case IR_ADD:        printf("\taddq %%rcx, %%rax\n"); break;
        case IR_ARG_PUSH:
            if (insn->param_index < 6) {
                printf("\tmovq %%rax, %%%s\n", x86_abi_regs[insn->param_index]);
            }
            break;
        case IR_CALL:       printf("\tcall %s\n", insn->label_name); break;
        case IR_RETURN:     printf("\tmovq %%rbp, %%rsp\n\tpopq %%rbp\n\tret\n"); break;
        case IR_FUNC_END:   printf("# End %s\n\n", insn->label_name); break;
        default: break;
    }
}

const target_ops_t x86_64_target = { .target_name = "x86_64", .emit = x86_emit };
const target_ops_t *g_target = &x86_64_target;
