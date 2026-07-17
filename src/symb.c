#include <string.h>
#include <stdlib.h>
#include <compiler.h>

static TSymb *g_TSymb_table = NULL;

void add_type(const char *name) {
    if (!name) return;
    
    TSymb *curr = g_TSymb_table;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return; 
        }
        curr = curr->next;
    }

    TSymb *new_sym = (TSymb *)malloc(sizeof(TSymb));
    if (new_sym) {
        new_sym->name = strdup(name);
        new_sym->next = g_TSymb_table;
        g_TSymb_table = new_sym;
    }
}

int is_registered_type(const char *name) {
    if (!name) return 0;
    
    TSymb *curr = g_TSymb_table;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return 1; // It's a user-defined type!
        }
        curr = curr->next;
    }
    return 0; // Not found
}
