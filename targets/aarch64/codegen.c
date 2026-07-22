#include <stdio.h>
#include <codegen.h>

static const char* aarch64_abi_regs[] = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};

void parser_init_hook(void) {
	// do nothing
}

static void aarch64_emit(const ir_insn_t *insn) {
    switch (insn->op) {
        case IR_FUNC_START:
            printf(".global %s\n%s:\n\tstp x29, x30, [sp, #-16]!\n\tmov x29, sp\n\tsub sp, sp, #256\n", insn->label_name, insn->label_name);
            break;

        case IR_PARAM_DECL: {
            const char *reg = "x0"; // Fallback
            if (insn->param_index >= 0 && insn->param_index < 8) {
                reg = aarch64_abi_regs[insn->param_index];
            }
            printf("\tstr %s, [x29, #%d] ; Bind %s to %s\n",
                    reg, insn->stack_offset, insn->label_name, reg);
            break;
        }

        case IR_LOAD_INT:
            printf("\tmov x0, #%ld\n", (long)insn->imm_val);
            break;

        case IR_LOAD_LOCAL:
            printf("\tldr x0, [x29, #%d]\n", insn->stack_offset);
            break;

        case IR_PUSH_TEMP:
            printf("\tstr x0, [sp, #-16]!\n");
            break;

        case IR_POP_TEMP:
            printf("\tldr x1, [sp], #16\n");
            break;

        case IR_ADD:
            printf("\tadd x0, x1, x0\n");
            break;

        case IR_ARG_PUSH:
            if (insn->param_index < 8) {
                printf("\tstr x0, [x29, #-%d] ; Park Arg %d\n", 192 - (insn->param_index * 8), insn->param_index);
            }
            break;

        case IR_CALL:
            printf("\tldr x0, [x29, #-192]\n");
            printf("\tldr x1, [x29, #-184]\n");
            printf("\tldr x2, [x29, #-176]\n");
            printf("\tldr x3, [x29, #-168]\n");
            printf("\tldr x4, [x29, #-160]\n");
            printf("\tldr x5, [x29, #-152]\n");
            printf("\tldr x6, [x29, #-144]\n");
            printf("\tldr x7, [x29, #-136]\n");
            printf("\tbl %s\n", insn->label_name);
            break;

        case IR_RETURN:
            printf("\tmov sp, x29\n\tldp x29, x30, [sp], #16\n\tret\n");
            break;

        case IR_FUNC_END:
            printf("// End %s\n\n", insn->label_name);
            break;

        case IR_SUB:
            printf("\tsub x0, x1, x0\n");
            break;

        case IR_LABEL:
            printf(".L%ld:\n", insn->imm_val);
            break;

        case IR_MUL:
            printf("\tmul x0, x0, x1\n");
            break;

        case IR_STORE_LOCAL:
            printf("\tstr x0, [x29, #%d]\n", insn->stack_offset);
            break;

        case IR_GLOBAL_DECL:
            printf(".data\n%s: .quad 0\n.text\n", insn->label_name);
            break;

        case IR_LOAD_GLOBAL:
            printf("\tadrp x1, %s\n", insn->label_name);
            printf("\tldr x0, [x1, #:lo12:%s]\n", insn->label_name);
            break;

        case IR_STORE_GLOBAL:
            printf("\tadrp x1, %s\n", insn->label_name);
            printf("\tstr x0, [x1, #:lo12:%s]\n", insn->label_name);
            break;

        case IR_CMP:
            printf("\tcmp x0, #%ld\n", insn->imm_val);
            break;

        case IR_JMP_Z:
            printf("\tb.eq .L%ld\n", insn->imm_val);
            break;

        case IR_EQ:
            printf("\tcmp x1, x0\n\tcset x0, eq\n"); break;
        case IR_NE:
            printf("\tcmp x1, x0\n\tcset x0, ne\n"); break;
        case IR_LT:
            printf("\tcmp x1, x0\n\tcset x0, lt\n"); break;
        case IR_GT:
            printf("\tcmp x1, x0\n\tcset x0, gt\n"); break;
        case IR_LE:
            printf("\tcmp x1, x0\n\tcset x0, le\n"); break;
        case IR_GE:
            printf("\tcmp x1, x0\n\tcset x0, ge\n"); break;

        case IR_INLINE_ASM:
            printf("\t%s\n", insn->label_name);
            break;

        case IR_JMP:
            printf("\tb .L%ld\n", insn->imm_val);
            break;

        case IR_LOAD_STRING:
            printf("\t.section .rodata\n");
            printf(".L_STR_%lx:\n", (unsigned long)insn->label_name);
            printf("\t.string \"%s\"\n", insn->label_name);
            printf("\t.text\n");
            printf("\tadrp x0, .L_STR_%lx\n", (unsigned long)insn->label_name);
            printf("\tadd x0, x0, #:lo12:.L_STR_%lx\n", (unsigned long)insn->label_name);
            break;

        case IR_LEA_LOCAL:
            if (insn->label_name) {
                printf("\tadrp x0, %s\n", insn->label_name);
                printf("\tadd x0, x0, #:lo12:%s\n", insn->label_name);
            } else {
                printf("\tadd x0, x29, #%d\n", insn->stack_offset);
            }
            break;

        case IR_LOAD_DEREF:
            printf("\tldr x0, [x0]\n");
            break;

        case IR_STORE_DEREF:
            printf("\tstr x0, [x1]\n");
            break;

        case IR_MEMBER_OFFSET:
            printf("\tadd x0, x0, #%ld\n", (long)insn->imm_val);
            break;

        case IR_INDEX_OFFSET: {
            long scale = insn->imm_val ? insn->imm_val : 8;
            printf("\tmov x2, #%ld\n", scale);
            printf("\tmul x0, x0, x2\n");
            printf("\tadd x0, x0, x1\n");
            break;
        }

        default:
            printf("; undefined opcode %d\n", insn->op);
            break;
    }
}

const target_ops_t aarch64_target = { .target_name = "aarch64", .emit = aarch64_emit };
const target_ops_t *g_target = &aarch64_target;
