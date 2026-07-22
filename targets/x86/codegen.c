#include <stdio.h>
#include <codegen.h>

void parser_init_hook(void) {
    // Hooks initialized if needed
}

static const char *x86_64_arg_regs[] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};

static void x86_emit(const ir_insn_t *insn) {
    switch (insn->op) {
        case IR_FUNC_START:
            printf(".global %s\n", insn->label_name);
            printf("%s:\n", insn->label_name);
            printf("\tpushq %%rbp\n");
            printf("\tmovq %%rsp, %%rbp\n");
            printf("\tsubq $256, %%rsp\n");
            break;

        case IR_PARAM_DECL: {
            const char *reg = "%rax";
            if (insn->param_index >= 0 && insn->param_index < 6) {
                reg = x86_64_arg_regs[insn->param_index];
            }
            printf("\tmovq %s, %d(%%rbp) # Bind %s to %s\n",
                   reg, insn->stack_offset, insn->label_name, reg);
            break;
        }

        case IR_LOAD_INT:
            printf("\tmovq $%ld, %%rax\n", (long)insn->imm_val);
            break;

        case IR_LOAD_LOCAL:
            printf("\tmovq %d(%%rbp), %%rax\n", insn->stack_offset);
            break;

        case IR_LEA_LOCAL:
            if (insn->label_name) {
                printf("\tleaq %s(%%rip), %%rax\n", insn->label_name);
            } else {
                printf("\tleaq %d(%%rbp), %%rax\n", insn->stack_offset);
            }
            break;

        case IR_PUSH_TEMP:
            printf("\tpushq %%rax\n");
            break;

        case IR_POP_TEMP:
            printf("\tpopq %%rcx\n");
            break;

        case IR_ADD:
            printf("\taddq %%rcx, %%rax\n");
            break;

        case IR_SUB:
            printf("\tsubq %%rax, %%rcx\n"); // rcx = rcx - rax (LHS - RHS)
            printf("\tmovq %%rcx, %%rax\n");
            break;

        case IR_MUL:
            printf("\timulq %%rcx, %%rax\n");
            break;

        case IR_STORE_LOCAL:
            printf("\tmovq %%rax, %d(%%rbp)\n", insn->stack_offset);
            break;

        case IR_ARG_PUSH:
            if (insn->param_index < 6) {
                int slot_offset = 192 - (insn->param_index * 8);
                printf("\tmovq %%rax, -%d(%%rbp) # Park arg %d\n", slot_offset, insn->param_index);
            }
            break;

	case IR_CALL: {
	    int arg_count = (int)insn->imm_val;
	    if (arg_count > 0) printf("\tmovq -192(%%rbp), %%rdi\n");
	    if (arg_count > 1) printf("\tmovq -184(%%rbp), %%rsi\n");
	    printf("\tcall %s\n", insn->label_name);
	    break;
	}

        case IR_RETURN:
            printf("\tmovq %%rbp, %%rsp\n");
            printf("\tpopq %%rbp\n");
            printf("\tret\n");
            break;

        case IR_FUNC_END:
            printf("# End %s\n\n", insn->label_name);
            break;

        case IR_LABEL:
            printf(".L%ld:\n", (long)insn->imm_val);
            break;

        case IR_GLOBAL_DECL:
            printf(".data\n%s: .quad 0\n.text\n", insn->label_name);
            break;

        case IR_LOAD_GLOBAL:
            printf("\tmovq %s(%%rip), %%rax\n", insn->label_name);
            break;

        case IR_STORE_GLOBAL:
            printf("\tmovq %%rax, %s(%%rip)\n", insn->label_name);
            break;

        case IR_CMP:
            if (insn->imm_val == 0) {
                printf("\ttestq %%rax, %%rax\n");
            } else {
                printf("\tcmpq $%ld, %%rax\n", (long)insn->imm_val);
            }
            break;

        case IR_JMP_Z:
            printf("\tje .L%ld\n", (long)insn->imm_val);
            break;

        case IR_JMP:
            printf("\tjmp .L%ld\n", (long)insn->imm_val);
            break;

        case IR_EQ:
            printf("\tcmpq %%rax, %%rcx\n\tsete %%al\n\tmovzbq %%al, %%rax\n");
            break;

        case IR_NE:
            printf("\tcmpq %%rax, %%rcx\n\tsetne %%al\n\tmovzbq %%al, %%rax\n");
            break;

        case IR_LT:
            printf("\tcmpq %%rax, %%rcx\n\tsetl %%al\n\tmovzbq %%al, %%rax\n");
            break;

        case IR_GT:
            printf("\tcmpq %%rax, %%rcx\n\tsetg %%al\n\tmovzbq %%al, %%rax\n");
            break;

        case IR_LE:
            printf("\tcmpq %%rax, %%rcx\n\tsetle %%al\n\tmovzbq %%al, %%rax\n");
            break;

        case IR_GE:
            printf("\tcmpq %%rax, %%rcx\n\tsetge %%al\n\tmovzbq %%al, %%rax\n");
            break;

        case IR_INLINE_ASM:
            printf("\t%s\n", insn->label_name);
            break;

        case IR_LOAD_STRING: {
            static long str_counter = 0;
            long id = str_counter++;
            printf("\t.section .rodata\n");
            printf(".L_STR_%ld:\n", id);
            printf("\t.string \"%s\"\n", insn->label_name);
            printf("\t.text\n");
            printf("\tleaq .L_STR_%ld(%%rip), %%rax\n", id);
            break;
        }

        case IR_LOAD_DEREF:
            if (insn->imm_val == 1) {
                printf("\tmovzbl (%%rax), %%eax\n");
            } else if (insn->imm_val == 2) {
                printf("\tmovzwl (%%rax), %%eax\n");
            } else if (insn->imm_val == 4) {
                printf("\tmovl (%%rax), %%eax\n");
            } else {
                printf("\tmovq (%%rax), %%rax\n");
            }
            break;

        case IR_STORE_DEREF:
            if (insn->imm_val == 1) {
                printf("\tmovb %%al, (%%rcx)\n");
            } else if (insn->imm_val == 2) {
                printf("\tmovw %%ax, (%%rcx)\n");
            } else if (insn->imm_val == 4) {
                printf("\tmovl %%eax, (%%rcx)\n");
            } else {
                printf("\tmovq %%rax, (%%rcx)\n");
            }
            break;

        case IR_MEMBER_OFFSET:
            printf("\taddq $%ld, %%rax\n", (long)insn->imm_val);
            break;

        case IR_INDEX_OFFSET: {
            long elem_size = insn->imm_val ? insn->imm_val : 8;
            printf("\timulq $%ld, %%rax\n", elem_size);
            printf("\taddq %%rcx, %%rax\n");
            break;
        }

        default:
            printf("; undefined opcode %d\n", insn->op);
            break;
    }
}

const target_ops_t x86_64_target = { .target_name = "x86_64", .emit = x86_emit };
const target_ops_t *g_target = &x86_64_target;
