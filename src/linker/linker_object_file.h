#include <elf/elf.h>
#include <base/base_inc.h>
#include <os/os_inc.h>
#include <sea/sea.h>
#include "linker.h"

#define ELF64_ST_INFO(bind, type)  (((bind) << 4) | ((type) & 0xf))

typedef struct LinkerStringEntryNode {
    String8 name;
    U64 output_idx;
    struct LinkerStringEntryNode *next;
} LinkerStringEntryNode;

// switch to a hashtable to resolve duplicates
typedef struct LinkerStringEntryList {
    LinkerStringEntryNode *first;
    LinkerStringEntryNode *last;
    U64 size;
    U64 section_idx;
} LinkerStringEntryList;

LinkerStringEntryList linker_new_string_entry_list(U64 section_idx) {
    return (LinkerStringEntryList) {
        .size = 1, // initial \0
    };
}

LinkerStringEntryNode *linker_add_string(Arena *arena, LinkerStringEntryList *strings, String8 name) {
    LinkerStringEntryNode *node = push_item(arena, LinkerStringEntryNode);
    if (strings->first == NULL) {
        strings->first = node;
    }
    strings->last->next = node;
    strings->last = node;
    node->name = name;
    node->output_idx = strings->size;
    strings->size += 1 + name.size; // add one for \0
    return node;
}


typedef struct LinkerSymbolNode {
    ELF_Sym64 sym;
    struct LinkerSymbolNode *next;
} LinkerSymbolNode;

typedef struct LinkerSymbolList {
    LinkerSymbolNode *first;
    LinkerSymbolNode *last;
    U64 count;
    LinkerStringEntryList *strings;
} LinkerSymbolList;


void linker_add_file_symbol (Arena *arena, LinkerSymbolList *lsyms, String8 filename) {
    LinkerSymbolNode *node = push_item(arena, LinkerSymbolNode);
    if (lsyms->first == NULL) {
        lsyms->first = node;
    }
    lsyms->last->next = node;
    lsyms->last = node;

    ELF_Sym64 *sym = &node->sym;
    sym->st_shndx = lsyms->strings->section_idx;
    sym->st_name = linker_add_string(arena, lsyms->strings, filename)->output_idx;
    sym->st_size = 0;
    sym->st_info = ELF64_ST_INFO(ELF_SymBind_Local, ELF_SymType_File);
    sym->st_other = ELF_SymVisibility_Default;
    sym->st_shndx = ELF_SectionIndex_Abs;
    sym->st_value = 0;

}

void linker_add_section_symbol (Arena *arena, LinkerSymbolList *lsyms, U64 section_idx, LinkerStringEntryNode *entry) {
    LinkerSymbolNode *node = push_item(arena, LinkerSymbolNode);
    if (lsyms->first == NULL) {
        lsyms->first = node;
    }
    lsyms->last->next = node;
    lsyms->last = node;

    ELF_Sym64 *sym = &node->sym;
    sym->st_shndx = section_idx;
    sym->st_name = entry->output_idx;
    sym->st_size = 0;
    sym->st_info = ELF64_ST_INFO(ELF_SymBind_Local, ELF_SymType_Section);
    sym->st_other = ELF_SymVisibility_Default;
    sym->st_shndx = ELF_SectionIndex_Abs;
    sym->st_value = 0;

}

void linker_add_function_symbol (Arena *arena, LinkerSymbolList *lsyms, U64 section_idx, SeaSymbolEntry *ssym) {
    if (ssym->type->kind != SeaLatticeKind_Func) {
        Assert(0);
    }
    LinkerSymbolNode *node = push_item(arena, LinkerSymbolNode);
    if (lsyms->first == NULL) {
        lsyms->first = node;
    }
    lsyms->last->next = node;
    lsyms->last = node;

    ELF_Sym64 *sym = &node->sym;
    sym->st_shndx = lsyms->strings->section_idx;
    sym->st_name = linker_add_string(arena, lsyms->strings, ssym->name)->output_idx;
    sym->st_size = 0; // TODO: track function size
    sym->st_info = ELF64_ST_INFO(ELF_SymBind_Global, ELF_SymType_Func);
    sym->st_other = ELF_SymVisibility_Default;
    sym->st_shndx = section_idx;
    sym->st_value = ssym->pos_in_section;

}


void linker_genobj_module (SeaModule *m) {
    rw_mutex_take_r(m->lock);
    Temp temp = scratch_begin(NULL, 1);

    ELF_Hdr64 obj_hdr = { 0 };
    MemoryCopyStruct(&obj_hdr, &default_header);
    obj_hdr.e_type = ELF_Type_Rel;
    obj_hdr.e_shentsize = sizeof (ELF_Shdr64);
    obj_hdr.e_shnum = 5; // NULL .text .strtab .shstrtab .symtab

    U64 offset = sizeof(ELF_Hdr64);

    LinkerStringEntryList section_list = linker_new_string_entry_list(4);

    ELF_Shdr64 shdrs[5] = {
        (ELF_Shdr64) { 0 },
        (ELF_Shdr64) {
            .sh_flags = ELF_Shf_Alloc | ELF_Shf_Alloc,
            .sh_offset = offset,
            .sh_type = ELF_ShType_ProgBits,
            .sh_addralign = 1,
            .sh_name =  linker_add_string(temp.arena, &section_list, str8_lit(".text"))->output_idx,
        },
        (E)
    };

    for EachNode(it, SeaSymbolEntry , m->symbols.first) {

    }

    scratch_end(temp);
    rw_mutex_drop_r(m->lock);

}
