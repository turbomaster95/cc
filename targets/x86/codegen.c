#include <stdio.h>
#include <codegen.h>

static const char* x86_abi_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static void x86_emit(const ir_insn_t *insn) {
    switch (insn->op) {
        case IR_FUNC_START:
            printf(".global %s\n%s:\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n\tsubq $256, %%rsp\n", insn->label_name, insn->label_name);
            break;
	case IR_PARAM_DECL: {
            char *reg = "%rax"; // Fallback
            if (insn->param_index == 0) reg = "%rdi";
            else if (insn->param_index == 1) reg = "%rsi";
            else if (insn->param_index == 2) reg = "%rdx";
            else if (insn->param_index == 3) reg = "%rcx";
            else if (insn->param_index == 4) reg = "%r8";
            else if (insn->param_index == 5) reg = "%r9";

            printf("\tmovq %s, %d(%%rbp) # Bind %s to %s\n", 
                    reg, insn->stack_offset, insn->label_name, reg);
            break;
        }
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
        case IR_SUB:
            printf("\tsubq %%rax, %%rcx\n");
            printf("\tmovq %%rcx, %%rax\n");
            break;
	case IR_LABEL:
            printf(".L%ld:\n", insn->imm_val);
            break;            
        case IR_MUL:
            printf("\timulq %%rcx, %%rax\n"); // rax = rax * rcx
            break;
	case IR_STORE_LOCAL:
            printf("\tmovq %%rax, %d(%%rbp)\n", insn->stack_offset);
            break;
	case IR_GLOBAL_DECL: {
            printf(".data\n%s: .quad 0\n.text\n", insn->label_name);
            break;
        }

        case IR_LOAD_GLOBAL: {
            printf("    movq %s(%%rip), %%rax\n", insn->label_name);
            break;
        }

        case IR_STORE_GLOBAL: {
            printf("    movq %%rax, %s(%%rip)\n", insn->label_name);
            break;
        }
        case IR_CMP:
            printf("\tcmpq $%ld, %%rax\n", insn->imm_val);
            break;
        case IR_JMP_Z:
            printf("\tje .L%ld\n", insn->imm_val);
            break;

        case IR_EQ:
            printf("\tcmpq %%rcx, %%rax\n");
            printf("\tsete %%al\n");
            printf("\tmovzbq %%al, %%rax\n");
            break;

        case IR_INLINE_ASM:
            printf("\t%s\n", insn->label_name); 
            break;

	case IR_JMP:
            printf("\tjmp .L%ld\n", insn->imm_val);
            break;
            
        case IR_NE:
            printf("\tcmpq %%rcx, %%rax\n\tsetne %%al\n\tmovzbq %%al, %%rax\n");
            break;
            
        case IR_LT:
            printf("\tcmpq %%rcx, %%rax\n\tsetl %%al\n\tmovzbq %%al, %%rax\n");
            break;
            
        case IR_GT:
            printf("\tcmpq %%rcx, %%rax\n\tsetg %%al\n\tmovzbq %%al, %%rax\n");
            break;
            
        case IR_LE:
            printf("\tcmpq %%rcx, %%rax\n\tsetle %%al\n\tmovzbq %%al, %%rax\n");
            break;
            
        case IR_GE:
            printf("\tcmpq %%rcx, %%rax\n\tsetge %%al\n\tmovzbq %%al, %%rax\n");
            break;

	case IR_LOAD_STRING:
            printf("\t.section .rodata\n");
            printf(".L_STR_%ld:\n", (long)insn->label_name); // We abuse the pointer as a unique ID
            printf("\t.string \"%s\"\n", insn->label_name);
            printf("\t.text\n");
            printf("\tleaq .L_STR_%ld(%%rip), %%rax\n", (long)insn->label_name);
            break;

        default: 
	    printf("; undefined opcode %d\n", insn->op);
            break;
    }
}

const target_ops_t x86_64_target = { .target_name = "x86_64", .emit = x86_emit };
const target_ops_t *g_target = &x86_64_target;
