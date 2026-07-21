#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <nu.h>

#define MEM_SIZE (1024 * 1024) // 1MB Pool
static uint8_t memory_pool[MEM_SIZE];

typedef enum {
    ASM_GCC,   /* Delegate assembly and linking together to gcc */
    ASM_GAS,   /* GNU 'as' */
    ASM_NASM   /* 'nasm' */
} AsmChoice;

typedef enum {
    STAGE_PREPROCESS, /* .c  */
    STAGE_COMPILE,    /* .i  */
    STAGE_ASSEMBLE,   /* .s  */
    STAGE_LINK        /* .o  */
} PipelineStage;

static bool run_cmd(char *const argv[]) {
    printf("[CMD]");
    for (int i = 0; argv[i] != NULL; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return false;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp failed");
        exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid failed");
        return false;
    }

    return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

static void replace_ext(const char *in, char *out, size_t out_sz, const char *new_ext) {
    snprintf(out, out_sz, "%s", in);
    char *dot = strrchr(out, '.');
    if (dot && dot != out) {
        *dot = '\0';
    }
    strncat(out, new_ext, out_sz - strlen(out) - 1);
}

static PipelineStage detect_start_stage(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return STAGE_PREPROCESS;

    if (strcmp(ext, ".i") == 0) return STAGE_COMPILE;
    if (strcmp(ext, ".s") == 0 || strcmp(ext, ".asm") == 0) return STAGE_ASSEMBLE;
    if (strcmp(ext, ".o") == 0) return STAGE_LINK;

    return STAGE_PREPROCESS;
}

int main(int argc, char *argv[]) {
    nu_mm_t *mm = nu_mm_create(NU_MM_ARENA, memory_pool, MEM_SIZE);

    if (!mm) {
        fprintf(stderr, "Failed to initialize memory manager");
        return 1;
    }

    nu_arg_parser_t *ap = nu_arg_create(mm);
    if (!ap) {
        fprintf(stderr, "Error: Failed to initialize argument parser\n");
        return 1;
    }

    /* Define options using nu_arg_def_t */
    nu_arg_def_t opt_E = { .short_flag = "-E", .long_flag = NULL, .type = NU_ARG_BOOL, .help = "Stop after preprocessing" };
    nu_arg_def_t opt_S = { .short_flag = "-S", .long_flag = NULL, .type = NU_ARG_BOOL, .help = "Stop after compilation" };
    nu_arg_def_t opt_c = { .short_flag = "-c", .long_flag = NULL, .type = NU_ARG_BOOL, .help = "Stop after assembly" };
    nu_arg_def_t opt_o = { .short_flag = "-o", .long_flag = NULL, .type = NU_ARG_STR,  .help = "Output binary/file target" };
    nu_arg_def_t opt_nasm = { .short_flag = NULL, .long_flag = "--use-nasm", .type = NU_ARG_BOOL, .help = "Use NASM assembler" };
    nu_arg_def_t opt_as   = { .short_flag = NULL, .long_flag = "--use-as",   .type = NU_ARG_BOOL, .help = "Use GNU 'as' assembler" };
    nu_arg_def_t opt_help = { .short_flag = "-h", .long_flag = "--help",     .type = NU_ARG_BOOL, .help = "Display help menu" };

    nu_arg_def_t *defs[] = { &opt_E, &opt_S, &opt_c, &opt_o, &opt_nasm, &opt_as, &opt_help };
    size_t def_count = sizeof(defs) / sizeof(defs[0]);

    for (size_t i = 0; i < def_count; i++) {
        nu_arg_register(ap, defs[i]);
    }

    if (!nu_arg_parse(ap, argc, argv)) {
        nu_arg_destroy(ap);
        return 1;
    }

    if (opt_help.is_set) {
        nu_arg_def_t print_defs[] = { opt_E, opt_S, opt_c, opt_o, opt_nasm, opt_as, opt_help };
        nu_arg_print_help(ap, print_defs, def_count);
        nu_arg_destroy(ap);
        return 0;
    }

    if (ap->positional_count < 1) {
        fprintf(stderr, "Error: No input file specified.\n");
        nu_arg_def_t print_defs[] = { opt_E, opt_S, opt_c, opt_o, opt_nasm, opt_as, opt_help };
        nu_arg_print_help(ap, print_defs, def_count);
        nu_arg_destroy(ap);
        return 1;
    }

    const char *input_file = ap->positionals[0];
    const char *output_file = opt_o.is_set ? opt_o.val.s : "a.out";

    AsmChoice asm_choice = ASM_GCC;
    if (opt_nasm.is_set) asm_choice = ASM_NASM;
    else if (opt_as.is_set) asm_choice = ASM_GAS;

    PipelineStage current_stage = detect_start_stage(input_file);

    char i_file[256], s_file[256], o_file[256];
    replace_ext(input_file, i_file, sizeof(i_file), ".i");
    replace_ext(input_file, s_file, sizeof(s_file), ".s");
    replace_ext(input_file, o_file, sizeof(o_file), ".o");

    bool created_i = false;
    bool created_s = false;
    bool created_o = false;

    /* -------------------------------------------------------------
     * STAGE 1: Preprocessor (cppc) -> .i
     * ------------------------------------------------------------- */
    if (current_stage == STAGE_PREPROCESS) {
        const char *pp_out = opt_E.is_set ? output_file : i_file;
        char *cpp_args[] = { "cppc", (char *)input_file, "-o", (char *)pp_out, NULL };

        if (!run_cmd(cpp_args)) {
            fprintf(stderr, "Error: Preprocessing failed.\n");
            nu_arg_destroy(ap);
            return 1;
        }
        if (opt_E.is_set) {
            nu_arg_destroy(ap);
            return 0;
        }

        created_i = true;
        current_stage = STAGE_COMPILE;
    }

    /* -------------------------------------------------------------
     * STAGE 2: Compiler (c99x86) -> .s
     * ------------------------------------------------------------- */
    if (current_stage == STAGE_COMPILE) {
        const char *comp_in = created_i ? i_file : input_file;
        const char *comp_out = opt_S.is_set ? output_file : s_file;

        char *c99_args[] = { "c99x86", (char *)comp_in, "-o", (char *)comp_out, NULL };
        if (!run_cmd(c99_args)) {
            fprintf(stderr, "Error: Compilation failed.\n");
            if (created_i) unlink(i_file);
            nu_arg_destroy(ap);
            return 1;
        }

        if (created_i) unlink(i_file);
        if (opt_S.is_set) {
            nu_arg_destroy(ap);
            return 0;
        }

        created_s = true;
        current_stage = STAGE_ASSEMBLE;
    }

    /* -------------------------------------------------------------
     * STAGE 3 & 4: Assembler & Linker
     * ------------------------------------------------------------- */
    if (asm_choice == ASM_GCC) {
        /* Direct assembly + linking via GCC driver */
        const char *gcc_in = created_s ? s_file : input_file;
        char *gcc_args[] = { "gcc", (char *)gcc_in, "-o", (char *)output_file, NULL };

        if (!run_cmd(gcc_args)) {
            fprintf(stderr, "Error: GCC assembly/link failed.\n");
            if (created_s) unlink(s_file);
            nu_arg_destroy(ap);
            return 1;
        }
    } else {
        /* Separate Assembler Stage */
        if (current_stage == STAGE_ASSEMBLE) {
            const char *asm_in = created_s ? s_file : input_file;
            const char *asm_out = opt_c.is_set ? output_file : o_file;
            bool asm_ok = false;

            if (asm_choice == ASM_NASM) {
                char *nasm_args[] = { "nasm", "-f", "elf64", (char *)asm_in, "-o", (char *)asm_out, NULL };
                asm_ok = run_cmd(nasm_args);
            } else { /* ASM_GAS */
                char *as_args[] = { "as", "--64", (char *)asm_in, "-o", (char *)asm_out, NULL };
                asm_ok = run_cmd(as_args);
            }

            if (!asm_ok) {
                fprintf(stderr, "Error: Assembly failed.\n");
                if (created_s) unlink(s_file);
                nu_arg_destroy(ap);
                return 1;
            }

            if (created_s) unlink(s_file);
            if (opt_c.is_set) {
                nu_arg_destroy(ap);
                return 0;
            }

            created_o = true;
            current_stage = STAGE_LINK;
        }

        /* Separate Linker Stage */
        if (current_stage == STAGE_LINK) {
            const char *link_in = created_o ? o_file : input_file;
            char *link_args[] = { "gcc", (char *)link_in, "-o", (char *)output_file, NULL };

            if (!run_cmd(link_args)) {
                fprintf(stderr, "Error: Linking failed.\n");
                if (created_o) unlink(o_file);
                nu_arg_destroy(ap);
                return 1;
            }

            if (created_o) unlink(o_file);
        }
    }

    if (created_s) unlink(s_file);
    nu_arg_destroy(ap);
    return 0;
}

