
#include "linker_error.h"
#include <linker/symbol_lookup.h>


static U32 new_hash(String8 name) {
    uint32_t h = 5381;
    for
        EachIndex(i, name.size) {
            h = h * 33 + name.str[i];
        }
    return h;
}

SymbolTable init_linking_table(U64 count) {
    SymbolTable table = { 0 };
    table.arena = arena_alloc();
    table.count = count;
    table.lookup = push_array(table.arena, SymbolNode *, table.count);
    return table;
}

internal SymbolNode *get_symbol(SymbolTable *table, String8 sym_name) {
    if (table->count == 0) {
        return NULL;
    }
    SymbolNode *node = NULL;
    U64 b_hash = new_hash(sym_name) % table->count;
    SymbolNode *bucket = table->lookup[b_hash];
    for (SymbolNode *it = bucket; it != NULL; it = it->next) {
        if (str8_match(it->sym_name, sym_name, 0)) {
            node = it;
            break;
        }
    }
    return node;
}

internal LinkerError add_symbol(Arena *arena, SymbolTable *table,
                                ElfFile *file, ELF_Sym64 *sym) {
    if (table->count == 0) {
        return LINKER_ERROR_GENERIC;
    }

    String8 sym_name = get_symbol_name(file, sym);
    SymbolNode *bucket = get_symbol(table, sym_name);
    if (bucket != NULL) {
        if (sym->st_shndx == ELF_SectionIndex_Undef) {
            return LINKER_ERROR_NONE;
        }

        U64 bucket_bind = ELF_ST_BIND(bucket->sym->st_info);
        switch (ELF_ST_BIND(sym->st_info)) {
        case ELF_SymBind_Global: {
            if (bucket_bind == ELF_SymBind_Global) {
                return LINKER_ERROR_SYM_DEFINED;
            }

        } break;
        case ELF_SymBind_Weak: {
            if (bucket_bind == ELF_SymBind_Global) {
                return LINKER_ERROR_SYM_DEFINED;
            }
            if (bucket_bind == ELF_SymBind_Weak) {
                return LINKER_ERROR_WEAK_SYM_DEFINED;
            }
        } break;
        }

    } else {
        SymbolNode *newNode = push_item(arena, SymbolNode);
        U64 idx = new_hash(sym_name) % table->count;
        SLLStackPush(table->lookup[idx], newNode);
        bucket = newNode;
    }
    bucket->file = file;
    bucket->sym = sym;
    bucket->sym_name = sym_name;
    return LINKER_ERROR_NONE;
}

void deinit_linking_table(SymbolTable *table) {
    arena_release(table->arena);
}
