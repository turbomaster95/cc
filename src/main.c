#include <nu.h>
#include <stdio.h>
#include <stdlib.h>
#include <compiler.h>
#include <codegen.h>

// Declare Bison globals
extern int yyparse(void);
extern FILE* yyin;

nu_mm_t *g_mm = NULL;
nu_ast_t *g_ast = NULL;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <source_file.c>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Failed to open input file");
        return 1;
    }
    yyin = file;

    size_t mem_size = 1024 * 1024 * 16; // 16 MB
    void *backing_mem = malloc(mem_size);
    g_mm = nu_mm_create(NU_MM_ARENA, backing_mem, mem_size);

    g_ast = nu_ast_create(g_mm);

    if (yyparse() == 0) {        
        if (g_ast->root) {
	    compile_ast(g_ast->root);
        }
    } else {
        printf("Parsing failed.\n");
	goto dead;
    }

    nu_ast_destroy(g_ast);
    nu_mm_destroy(g_mm);
    free(backing_mem);
    fclose(file);
    return 0;

dead:
    nu_ast_destroy(g_ast);
    nu_mm_destroy(g_mm);
    free(backing_mem);
    fclose(file);
    return -1;
}
