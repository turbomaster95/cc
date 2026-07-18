#include <nu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compiler.h>
#include <codegen.h>

extern int yyparse(void);
extern FILE* yyin;
extern void set_parser_ast_target(nu_ast_t *ast);

nu_mm_t *g_mm = NULL;
nu_ast_t *g_ast = NULL;

int g_c11_enabled = 0;

static void print_help(const char *prog_name) {
    printf("Usage: %s [options] <source_file>\n\n", prog_name);
    printf("Options:\n");
    printf("  -std, --std <standard>  The C Language standard to use\n");
    printf("  -h, --help              Show this help message\n");
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    size_t mem_size = 1024 * 1024 * 16; // 16 MB
    void *backing_mem = malloc(mem_size);
    g_mm = nu_mm_create(NU_MM_ARENA, backing_mem, mem_size);

    if (!g_mm) {
        fprintf(stderr, "Failed to create memory manager\n");
        return EXIT_FAILURE;
    }

    nu_arg_parser_t *ap = nu_arg_create(g_mm);
    if (!ap) {
        nu_mm_destroy(g_mm);
        return EXIT_FAILURE;
    }

    nu_arg_def_t std_opt = {
        .type = NU_ARG_STR,
        .short_flag = "-std",
        .long_flag = "--std",
        .help = "The C Language standard to use",
        .is_set = false,
        .val.s = NULL
    };

    nu_arg_register(ap, &std_opt);

    if (!nu_arg_parse(ap, argc, argv)) {
        nu_arg_destroy(ap);
        nu_mm_destroy(g_mm);
        return EXIT_FAILURE;
    }

    // Set C11 mode based on -std flag
    if (std_opt.is_set && std_opt.val.s) {
        if (strcmp(std_opt.val.s, "c11") == 0 || 
            strcmp(std_opt.val.s, "gnu11") == 0) {
            g_c11_enabled = 1;
        } else if (strcmp(std_opt.val.s, "c99") != 0 && 
                   strcmp(std_opt.val.s, "gnu99") != 0 &&
                   strcmp(std_opt.val.s, "c89") != 0 &&
                   strcmp(std_opt.val.s, "c90") != 0) {
            fprintf(stderr, "Error: Unsupported standard '%s'\n", std_opt.val.s);
            nu_arg_destroy(ap);
            nu_mm_destroy(g_mm);
            return EXIT_FAILURE;
        }
    }

    const char *source_path = NULL;
    if (ap->positional_count > 0) {
        source_path = ap->positionals[0];
    }

    if (!source_path) {
        fprintf(stderr, "Error: No input file specified\n\n");
        print_help(argv[0]);
        nu_arg_destroy(ap);
        nu_mm_destroy(g_mm);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(source_path, "r");
    if (!file) {
        perror("Failed to open input file");
        nu_arg_destroy(ap);
        nu_mm_destroy(g_mm);
        return EXIT_FAILURE;
    }

    yyin = file;
    g_ast = nu_ast_create(g_mm);

    if (yyparse() == 0) {
        if (g_ast->root) {
            compile_ast(g_ast->root);
        }
    } else {
        printf("Parsing failed.\n");
        fclose(file);
        nu_ast_destroy(g_ast);
        nu_arg_destroy(ap);
        nu_mm_destroy(g_mm);
        return EXIT_FAILURE;
    }

    fclose(file);
    nu_ast_destroy(g_ast);
    nu_arg_destroy(ap);
    nu_mm_destroy(g_mm);
    return EXIT_SUCCESS;
}
