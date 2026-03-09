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

typedef struct LinkingTable {
    Arena *arena;
    SymbolNode **lookup;
    U64 count;
} LinkingTable;

LinkingTable init_linking_table(U64 count);

internal SymbolNode *get_symbol(LinkingTable *table, String8 sym_name);

internal LinkerError add_symbol(Arena *arena, LinkingTable *table,
                                ElfFile *file, ELF_Sym64 *sym);

void deinit_linking_table(LinkingTable *table);

#endif // LINKER_SYMBOL_LOOKUP_H
