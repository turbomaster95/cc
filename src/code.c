#include <codegen.h>

void emit_ir(const ir_insn_t *insn) {
    if (g_target && g_target->emit) {
        g_target->emit(insn);
    }
}
