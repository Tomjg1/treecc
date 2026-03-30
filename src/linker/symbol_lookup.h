#ifndef LINKER_SYMBOL_LOOKUP_H
#define LINKER_SYMBOL_LOOKUP_H
#include <linker/parser.h>
#include <linker/linker_error.h>
#include <base/base_inc.h>

/*
typedef struct LocationNode LocationNode;
struct LocationNode {
    ElfFile *file;
    ELF_Rela64 *rela;
    LocationNode *next;
};
*/

typedef struct SymbolNode SymbolNode;
struct SymbolNode {
    // point
    String8 sym_name;
    ELF_Sym64 *sym;
    ElfFile *file;
    // HT chain
    SymbolNode *next;
};

typedef struct SymbolTable {
    Arena *arena;
    SymbolNode **lookup;
    U64 count;
} SymbolTable;

SymbolTable init_linking_table(U64 count);

internal SymbolNode *get_symbol(SymbolTable *table, String8 sym_name);

internal LinkerError add_symbol(Arena *arena, SymbolTable *table,
                                ElfFile *file, ELF_Sym64 *sym);

void deinit_linking_table(SymbolTable *table);

#endif // LINKER_SYMBOL_LOOKUP_H
