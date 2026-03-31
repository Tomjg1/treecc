#include "symbol_lookup.h"
#include <linker/executable_gen.h>

/*

.init
.text
.text.*


.init
.fini
.text
.text.*

ELF file is opened.

Sections are loaded into a trie based on their name,
unmatched flags are moved into a section based on there flags
output sections are grouped into output segments
output segments are written to disk


How to handle merge sections?

merge sections should be merged with matching sections


 */

typedef enum {
    TRIE_PREFIX_MATCH_MISMATCH = 0,
    TRIE_PREFIX_MATCH_SHORT,
    TRIE_PREFIX_MATCH_FULL,
    TRIE_PREFIX_MATCH_EQUAL,
} TriePrefixMatch;

typedef struct TriePrefixMatchResult{
    TriePrefixMatch type;
    U64 offset;
} TriePrefixMatchResult;

TriePrefixMatchResult trie_string8_match_prefix (String8 prefix, String8 other) {
    TriePrefixMatchResult res = { 0 };
    U64 len = Min(prefix.size, other.size);
    U64 i = 0;
    for (; i < len ;i++) {
        if (prefix.str[i] != other.str[i]) {
            res.type = TRIE_PREFIX_MATCH_MISMATCH;
            res.offset = i;
            return res;
        }
    }
    res.offset = i;
    if (other.size > prefix.size){
        res.type = TRIE_PREFIX_MATCH_FULL;
    } else if (other.size < prefix.size) {
        res.type = TRIE_PREFIX_MATCH_SHORT;
    } else {
        res.type = TRIE_PREFIX_MATCH_EQUAL;
    }

    return res;
}

typedef enum {
    TRIE_MATCH_WILDCARD,
    TRIE_MATCH_EXACT,
    TRIE_MATCH_NONE,
} TrieMatch;


typedef struct TrieNode {
    String8 prefix; // something like .text.
    void *value;
    TrieMatch type;
    struct TrieNode **lookup;
    struct TrieNode *neighbor;
    U64 count;
} TrieNode;

typedef struct Trie {
    TrieNode *root;
    Arena *arena;
} Trie;

Trie trie_init() {
    return (Trie) {
        .arena = arena_alloc(),
        .root = NULL,
    };
}

void trie_release(Trie *trie) {
    arena_release( trie->arena);
    trie->arena = NULL;
    trie->root = NULL;
}

#define TRIE_LOOKUP_LENGTH 16
#define TRIE_LOOKUP_MASK (TRIE_LOOKUP_LENGTH - 1)
#define TRIE_HASH(c) ((c) & (TRIE_LOOKUP_LENGTH))

TrieNode *trie_node_lookup(TrieNode *node, char key) {
    if (node->lookup == NULL) {
        return NULL;
    }
    U64 hash = TRIE_HASH(key);
    TrieNode *cnode = node->lookup[hash];
    while (cnode != NULL) {
        if (cnode->prefix.str[0] != key) {
            break;
        }
        cnode = cnode->neighbor;
    }
    return cnode;
}

TriePrefixMatchResult trie_prefix_search(TrieNode **node, String8 *key) {
    // check prefix
    TrieNode *cnode = *node;
    String8 ckey = *key;
    TriePrefixMatchResult match = { 0 };
    while (cnode != NULL) {
        match = trie_string8_match_prefix(cnode->prefix, ckey);
        if (match.type != TRIE_PREFIX_MATCH_FULL) {
            break;
        }
        ckey = str8_skip(ckey, cnode->prefix.size);
        cnode = trie_node_lookup(cnode, ckey.str[0]);
    }
    *node = cnode;
    *key = ckey;

    return match;
}


TrieNode *trie_node_alloc(Trie *trie, String8 key, void *value, TrieMatch match_type) {
    TrieNode *node = push_item(trie->arena, TrieNode);
    node->prefix = key;
    node->type = match_type;
    node->value = value;
    return node;
}

void trie_node_insert(Trie *trie, TrieNode *node, TrieNode *child){
    if (node->lookup == NULL) {
        node->lookup = push_array(trie->arena, TrieNode *, TRIE_LOOKUP_LENGTH);
        node->count = TRIE_LOOKUP_LENGTH;
    }
    U64 hash = TRIE_HASH(child->prefix.str[0]);
    TrieNode **bucket = &node->lookup[hash];
    while ((*bucket) != NULL) {
        bucket = &(*bucket)->neighbor;
    }
    *bucket = child;

}

/*
 *
 switch (match.type) {
 case TRIE_PREFIX_MATCH_SHORT: {
 }break;
 case TRIE_PREFIX_MATCH_MISMATCH: {

 }break;
 case TRIE_PREFIX_MATCH_FULL: {}break;
 case TRIE_PREFIX_MATCH_EQUAL: {}break;
 }
 */

TrieNode *trie_lookup(Trie *trie, String8 key) {
    TrieNode *node = trie->root;
    TriePrefixMatchResult res = trie_prefix_search(&node, &key);
    switch (res.type) {
    case TRIE_PREFIX_MATCH_SHORT:
    case TRIE_PREFIX_MATCH_MISMATCH: {
    }break;
    case TRIE_PREFIX_MATCH_FULL: {
        if (node->type != TRIE_MATCH_NONE){
            return node;
        }
    }break;
    case TRIE_PREFIX_MATCH_EQUAL: {
        if (node->type == TRIE_MATCH_WILDCARD) {
            return node;
        }
    }break;
    }
    return NULL;

}

TrieNode *trie_insert(Trie *trie, String8 key, void *value, TrieMatch match_type) {
    if (trie->root == NULL) {
        trie->root = trie_node_alloc(trie, key, value, match_type);
        return trie->root;
    }

    TrieNode *node = trie->root;
    TriePrefixMatchResult res = trie_prefix_search(&node, &key);

    switch (res.type) {
        case TRIE_PREFIX_MATCH_EQUAL: {
            node->value = value;
            node->type = match_type;
            return node;
        }
        case TRIE_PREFIX_MATCH_FULL: {
            TrieNode *child = trie_node_alloc(trie, key, value, match_type);
            trie_node_insert(trie, node, child);
            return child;
        }
        case TRIE_PREFIX_MATCH_SHORT: {
            String8 old_suffix = str8_skip(node->prefix, key.size);

            TrieNode *child = trie_node_alloc(trie, old_suffix, node->value, node->type);
            child->lookup = node->lookup;
            child->count = node->count;

            node->prefix = key;
            node->value = value;
            node->type = match_type;
            node->lookup = NULL;
            node->count = 0;
            trie_node_insert(trie, node, child);
            return node;
        }
        case TRIE_PREFIX_MATCH_MISMATCH: {
            String8 common = str8(node->prefix.str, res.offset);
            String8 old_suffix = str8_skip(node->prefix, res.offset);
            String8 new_suffix = str8_skip(key, res.offset);

            TrieNode *old_child = trie_node_alloc(trie, old_suffix, node->value, node->type);
            old_child->lookup = node->lookup;
            old_child->count = node->count;

            TrieNode *new_child = trie_node_alloc(trie, new_suffix, value, match_type);

            node->prefix = common;
            node->value = NULL;
            node->type = TRIE_MATCH_NONE;
            node->lookup = NULL;
            node->count = 0;
            trie_node_insert(trie, node, old_child);
            trie_node_insert(trie, node, new_child);
            return new_child;
        }
    }
    return NULL;
}

typedef struct ElfSection_list {
    ElfSection *section; // ElfSection has final offsets for every section involved (points to input_files)
    ElfFile *file;
    struct ElfSection_list *next;
} ElfSection_list;

typedef struct InputSection {// OutputSections will be added to the trie for loading
    String8 name;
    U64 flags;
    U64 type;
    ElfSection_list *next;
    ElfSection_list *tail;
} InputSection;

typedef struct LinkerSection {
    String8 name;
    U64 flags;
    U64 type;
    InputSection *next;
    InputSection *tail;
} LinkerSection;

typedef struct LinkerSegment {
    U64 flags;
    U64 type;
} LinkerSegment;

typedef struct LinkerAlignment {
    U64 alignment;
} LinkerAlignment;

typedef struct LinkerSymbol {
    String8 name;
    U64 type;
    U64 size;

} LinkerSymbol;

typedef enum LinkerObjType{
    LinkerObjType_SEGMENT,
    LinkerObjType_SECTION,
    LinkerObjType_ALIGNMENT,
    LinkerObjType_SYMBOL,
    LinkerObjType_COUNT,
} LinkerObjType;

typedef struct LinkerObject {
    LinkerObjType type;
    struct LinkerObject *parent;
    struct LinkerObject *next;
    struct LinkerObject *prev;
    struct LinkerObject *child;
    union {
        LinkerSegment segment;
        LinkerSection section;
        LinkerAlignment align;
        LinkerSymbol symbol;
    } obj;
} LinkerObject;

void linker_object_add_neighbor(LinkerObject *node, LinkerObject *neighbor) {
    neighbor->parent = node->parent; // they share parents
    neighbor->prev = node;
    neighbor->next = node->next;
    node->next->prev = neighbor;
    node->next = neighbor;

}

void linker_object_add_child(LinkerObject *parent, LinkerObject *child) {
    child->parent = parent;
    if (parent->child == NULL) {
        parent->child = child;
        child->next = child;
        child->prev = child;
    } else {
        child->prev = parent->child->prev;
        child->prev->next = child;
        child->next = parent->child;
        parent->child->prev = child;
    }
}

LinkerObject *linker_object_remove_child(LinkerObject *parent, LinkerObject *child) {
    if (parent->child == NULL || child->parent != parent) {
        return NULL;
    }
    if (child->next == child) {
        parent->child = NULL;
    } else {
        child->prev->next = child->next;
        child->next->prev = child->prev;
        if (child == parent->child)
            parent->child = child->next;
    }

    child->parent = NULL;
    child->next = NULL;
    child->prev = NULL;
}

typedef struct LinkerExeOutput {
    Arena *arena;
    ElfFile_array *input_files;
    LinkerObject *root;
    LinkerObject *cursor;
    SymbolTable global_table;
    Trie section_trie;
    LinkerObject *freeList;
    U64 phdr_count;
} LinkerExeOutput;

LinkerExeOutput linker_exe_output_init(void) {
    return (LinkerExeOutput) {
        .arena = arena_alloc(),
        .section_trie = trie_init(),
    };
}

LinkerObject *linker_exe_output_alloc_obj(LinkerExeOutput *output) {
    LinkerObject *ret = NULL;
    if (output->freeList != NULL) {
        ret = output->freeList;
        output->freeList = output->freeList->next;
    } else {
        ret = push_item(output->arena, LinkerObject);
    }
    return ret;
}

void linker_exe_start_segment(LinkerExeOutput *output, U64 type, U64 flags) {
    if (output->root == NULL) {
        output->root = linker_exe_output_alloc_obj(output);
        output->cursor = output->root;
    } else {
        LinkerObject *new = linker_exe_output_alloc_obj(output);
        linker_object_add_child(output->cursor, new);
        output->cursor = new;
    }
    output->phdr_count++;
    output->cursor->type = LinkerObjType_SEGMENT,
    output->cursor->obj.segment =
        (LinkerSegment) {
            .type = type,
            .flags = flags,
        };
}

void linker_exe_end_segment(LinkerExeOutput *output) { // TODO: add Error handling
    if (output->cursor == NULL) {
        Assert(0);
    }
    if (output->cursor->type != LinkerObjType_SEGMENT) {
        Assert(0);
    }
    output->cursor = output->cursor->parent;
}

void linker_exe_start_section(LinkerExeOutput *output, String8 key) { // TODO: add Error handling
    if (output->cursor == NULL) {
        Assert(0);
    }
    if (output->cursor != LinkerObjType_SEGMENT) {
        Assert(0);
    }

}

/*
test = linker_exe_init(void)
linker_exe_start_segment(alloc, readable, executable)
linker_exe_add_section(".init")
linker_exe_start_output_section(".text")
linker_exe_add_section(".text")
linker_exe_add_section(".text.*")
linker_exe_end_output_section()
linker_exe_add_section(".fini")
linker_exe_add_alignment(PAGESIZE)
linker_exe_end_segment()
linker_exe_start_segment(alloc, read, write)
linker_exe_start_relro_segment()
linker_exe_add_section(".tdata")
linker_add_section(".tbss")
linker_exe_add_alignment(PAGESIZE)
linker_exe_end_relro_segment()
linker_add_section(".data")
linker_add_section(".bss")
linker_exe_add_alignment(PAGESIZE)
linker_exe_end_segment()
linker_exe_end_segment()
linker_exe_stack_section(".note.gnustack")


 */



static U32 segment_flags_from_section(U64 sh_flags) {
    U32 flags = 0;
    if (sh_flags & ELF_Shf_Alloc) flags |= ELF_PFlag_Read;
    if (sh_flags & ELF_Shf_ExecInstr) flags |= ELF_PFlag_Exec;
    if (sh_flags & ELF_Shf_Write) flags |= ELF_PFlag_Write;
    return flags;
}

static U8 get_loadable_segment_order(U64 flags) {
    U8 out;
    if (!(flags & ELF_Shf_Alloc)) {
        Assert(0); // non Allocatables cannot be included.
    } else if (flags & ELF_Shf_ExecInstr) {
        out = 1; // RX
    } else if (flags & ELF_Shf_Write) {
        out = 3; // RW
    } else {
        out = 2; // R
    }
    return out;
}

void load_input_sections(Arena *arena, ElfFile *file, OutputElfExe *out_file) {
    for EachIndex(i, file->shdrs.count) {
        if (!(file->shdrs.v[i].hdr.sh_flags & ELF_Shf_Alloc)) { // limited exe only needs Alloc. sections <-- possibly add header data to it's own page for simplicity
            continue;
        }
        if (
            (file->shdrs.v[i].hdr.sh_flags & (ELF_Shf_Tls || ELF_Shf_ExecInstr)) // executable TLS
            == (ELF_Shf_Tls || ELF_Shf_ExecInstr)) {
            continue; // TODO pass info back to user that this combo is odd
        }

        String8 sec_name = get_section_name(file, &file->shdrs.v[i].hdr);
        U64 sec_flags = file->shdrs.v[i].hdr.sh_flags;
        U64 sec_type = file->shdrs.v[i].hdr.sh_type;

        OutputSections **it = NULL;
        out_file->output[get_loadable_segment_order(sec_flags)].flags = sec_flags;
        if (sec_type == ELF_ShType_NoBits) {
            it = &out_file->output[get_loadable_segment_order(sec_flags)].no_bit_sections;
        } else {
            it = &out_file->output[get_loadable_segment_order(sec_flags)].prog_sections;
            if (*it == NULL) {
                out_file->segment_count++;
            }
        }
        while (*it != NULL && str8_is_before(sec_name, (*it)->section_name)) {
            it = &(*it)->next;
        }
        if (*it == NULL || !str8_match(sec_name, (*it)->section_name, 0)) {
            OutputSections *section = push_item(arena, OutputSections);
            section->section_name = sec_name;
            section->next = *it;
            *it = section;
        }
        ElfSection_list *new_section = push_item(arena, ElfSection_list);
        new_section->section = &file->shdrs.v[i];
        new_section->file = file;
        new_section->next = (*it)->sections;
        (*it)->sections = new_section;

    }
}

//default elf header layout
read_only ELF_Hdr64 default_header =
(ELF_Hdr64){
    .e_ident = {
        [ELF_Identifier_Mag0] = 0x7f,
        [ELF_Identifier_Mag1] = 'E',
        [ELF_Identifier_Mag2] = 'L',
        [ELF_Identifier_Mag3] = 'F',
        [ELF_Identifier_Class] = ELF_Class_64,
        [ELF_Identifier_Data] = ELF_Data_2LSB,
        [ELF_Identifier_Version] = ELF_Version_Current,
        [ELF_Identifier_OsAbi] = 0,
    },
    .e_type = ELF_Type_None,
    .e_machine = ELF_MachineKind_X86_64,
    .e_version = ELF_Version_Current,
    .e_entry = 0,
    .e_phoff = 0,
    .e_shoff = 0,
    .e_flags = 0,
    .e_ehsize = sizeof(ELF_Hdr64),
    .e_phentsize = sizeof(ELF_Phdr64),
    .e_phnum = 0,
    .e_shentsize = sizeof(ELF_Shdr64),
    .e_shnum = 0,
    .e_shstrndx = ELF_SectionIndex_Undef,
};

internal OutputElfExe buildOutputElfFile(Arena *arena, ElfFile_array *array, U64 section_alignment, U64 base_image_address) {
    OutputElfExe output = {0};
    // fill section list
    {
        for EachIndex(i, array->count) {
            load_input_sections(arena, &array->v[i], &output);
        }
    }
    // compute offsets, fill linking tables
    {

        output.global_table = init_linking_table(100);
        U64 current_mem_base = base_image_address; //
        U64 current_file_base = 0; //
        U64 current_file_offset =  sizeof(ELF_Hdr64) + sizeof(ELF_Phdr64) * output.segment_count; //reserve space for headers
        U64 current_mem_offset =  current_file_offset + base_image_address;
        for EachIndex(i, ArrayCount(output.output)) {
            OutputSegment *segment = &output.output[i];
            if (segment->prog_sections != NULL) {
                for EachNode(j, OutputSections, segment->prog_sections) {
                    for EachNode(it, ElfSection_list, j->sections) {
                        current_mem_offset = AlignPow2(current_mem_offset, it->section->hdr.sh_addralign);
                        it->section->memory_offset = current_mem_offset;

                        current_file_offset = AlignPow2(current_file_offset, it->section->hdr.sh_addralign);
                        it->section->file_offset = current_file_offset;
                        current_file_offset += it->section->hdr.sh_size;

                        current_mem_offset += it->section->hdr.sh_size;

                        for EachIndex(k, it->file->syms.count) {
                            // load symbol tables
                            if (ELF_ST_BIND(it->file->syms.v[k].st_info) == ELF_SymBind_Global ) {
                                add_symbol(arena, &output.global_table, it->file, &it->file->syms.v[k]); // <------ type is also a key, currently only filters name. TODO: handle name collisions by comparing types.
                            }
                        }
                    }
                }
            }
            if (segment->no_bit_sections != NULL) {
                for EachNode(j, OutputSections, segment->prog_sections) {
                    for EachNode(it, ElfSection_list, j->sections) {
                        current_mem_offset = AlignPow2(current_mem_offset, it->section->hdr.sh_addralign);
                        it->section->memory_offset = current_mem_offset;

                        current_mem_offset += it->section->hdr.sh_size;

                        for EachIndex(k, it->file->syms.count) {
                            // load symbol tables
                            if (ELF_ST_BIND(it->file->syms.v[k].st_info) == ELF_SymBind_Global ) {
                                add_symbol(arena, &output.global_table, it->file, &it->file->syms.v[k]); // <------ type is also a key, currently only filters name. TODO: handle name collisions by comparing types.
                            }
                        }
                    }
                }
            }
            ELF_Phdr64 phdr = {0};
            phdr.p_align = section_alignment;
            phdr.p_filesz = current_file_offset - current_file_base;
            phdr.p_offset = current_file_base;
            phdr.p_vaddr = current_mem_base;
            phdr.p_paddr = current_mem_base;
            phdr.p_memsz = current_mem_offset - current_mem_base;
            phdr.p_flags = segment_flags_from_section(segment->flags);
            phdr.p_type = ELF_PType_Load;

            current_mem_base = AlignPow2(current_mem_offset, section_alignment);
            current_file_base = AlignPow2(current_file_offset, section_alignment);
            current_mem_offset = current_mem_base;
            current_file_offset = current_file_base;
            segment->phdr = phdr;
        }
    }
    return output;
}

String8 load_section_data(Arena *arena, ElfFile *file, ELF_Shdr64 *shdr) {
    String8 data = { 0 };
    U8 *data_ptr = push_array(arena, U8, shdr->sh_size);
    Rng1U64 rng = rng_1u64(shdr->sh_offset, shdr->sh_offset + shdr->sh_size);
    U64 bytes_read = os_file_read(file->handle, rng, data_ptr);
    if (bytes_read == shdr->sh_size) {
        data.size = shdr->sh_size;
        data.str = data_ptr;
    }
    return data;
}


void apply_all_relocations(SymbolTable *global_table, ElfFile *file, ElfSection *shdr, String8 sec_data){

    for EachNode(it, ElfRela64_Node, shdr->relas) {
        for EachIndex(i, it->relas.count) {
            U64 sym_idx = ELF64_R_SYM(it->relas.v[i].r_info);
            if (sym_idx < file->syms.count) {
                ELF_Sym64 *sym = NULL;
                ElfFile *sym_file = NULL;
                ELF_SymBind binding = ELF_ST_BIND(file->syms.v[sym_idx].st_info);
                if (binding == ELF_SymBind_Local) {
                    sym = &file->syms.v[sym_idx];
                    sym_file = file;
                } else {
                    String8 sym_name = get_reloc_name(file, &it->relas.v[i]);
                    SymbolNode *node = get_symbol(global_table,  sym_name);
                    sym = node->sym;
                    sym_file = node->file;
                }
                U64 S = 0;
                {
                    if (sym->st_shndx != ELF_SectionIndex_Abs) {
                        S = sym_file->shdrs.v[sym->st_shndx].memory_offset + sym->st_value;
                    } else {
                        S = sym->st_value;
                    }
                }
                S64 A = it->relas.v[i].r_addend;
                U64 P = shdr->memory_offset + it->relas.v[i].r_offset;
                switch (ELF64_R_TYPE(it->relas.v[i].r_info)) {
                case ELF_RelocX8664_64: {
                    *(U64 *)(sec_data.str + it->relas.v[i].r_offset) = S + A;
                }break;
                case ELF_RelocX8664_Plt32:{
                     *(U32 *)(sec_data.str + it->relas.v[i].r_offset) = (U32)((S + A) - P);
                }break;
                case ELF_RelocX8664_Pc32: {
                    *(S32 *)(sec_data.str + it->relas.v[i].r_offset) = (S32)((S + A) - P);
                }break;
                }
            }
        }
    }
}




#define rng_append(r, a) (Rng1U64)rng_1u64((r).max,(r).max + a)

internal void build_elf_exe(Arena *arena, OutputElfExe *output,
                                   String8 output_filename) {
    // write header and segment data
    OS_Handle output_file = os_file_open(OS_AccessFlag_Write, output_filename);

    Temp temp = temp_begin(arena);


    ELF_Hdr64 hdr = {0};
    MemoryCopyStruct(&hdr, &default_header);

    hdr.e_phoff = sizeof(ELF_Hdr64);
    hdr.e_phnum = output->segment_count;
    U64 entry_address = 0;
    {
        SymbolNode *entry_symbol = get_symbol(&output->global_table, str8_lit("_start"));
        U64 st_offset = entry_symbol->file->shdrs.v[entry_symbol->sym->st_shndx].memory_offset;
        entry_address = entry_symbol->sym->st_value + st_offset;
    }
    hdr.e_entry = entry_address;
    hdr.e_type = ELF_Type_Exec;
    Rng1U64 rng = rng_1u64(0, sizeof(ELF_Hdr64));
    os_file_write(output_file, rng, &hdr);
    temp_end(temp);
    // load file section data
    for EachIndex(i, ArrayCount(output->output)) {
        OutputSegment segments = output->output[i];

        for EachNode(it, OutputSections, segments.prog_sections) {
            for EachNode (ot, ElfSection_list, it->sections) {
                if (ot->section->hdr.sh_type == ELF_ShType_NoBits) continue;
                Temp temp = temp_begin(arena);
                String8 data = load_section_data(temp.arena, ot->file, &ot->section->hdr);
                // apply relocations
                apply_all_relocations(&output->global_table, ot->file, ot->section, data);
                os_file_write(output_file, rng_1u64(ot->section->file_offset, ot->section->file_offset + data.size), data.str);
                temp_end(temp);
            }
        }
    }
    for EachIndex(i, ArrayCount(output->output)) {
        rng = rng_append(rng, sizeof(output->output[i].phdr));
        os_file_write(output_file, rng, &output->output[i].phdr);
    }
}
