#include "parser.h"
#include "symbol_lookup.h"
#include <linker/executable_gen.h>

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
#define TRIE_HASH(c) ((c) & (TRIE_LOOKUP_MASK))

TrieNode *trie_node_lookup(TrieNode *node, char key) {
    if (node->lookup == NULL) {
        return NULL;
    }
    U64 hash = TRIE_HASH(key);
    TrieNode *cnode = node->lookup[hash];
    while (cnode != NULL) {
        if (cnode->prefix.str[0] == key) {
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
        *node = cnode;
        if (match.type != TRIE_PREFIX_MATCH_FULL) {
            break;
        }
        ckey = str8_skip(ckey, cnode->prefix.size);
        cnode = trie_node_lookup(cnode, ckey.str[0]);
    }
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


TrieNode *trie_lookup(Trie *trie, String8 key) {
    TrieNode *node = trie->root;
    TriePrefixMatchResult res = trie_prefix_search(&node, &key);
    switch (res.type) {
    case TRIE_PREFIX_MATCH_SHORT:
    case TRIE_PREFIX_MATCH_MISMATCH: {
    }break;
    case TRIE_PREFIX_MATCH_EQUAL: {
        if (node->type != TRIE_MATCH_NONE){
            return node;
        }
    }break;
    case TRIE_PREFIX_MATCH_FULL: {
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
    child->next = child;
    child->prev = child;
    return child;
}

LinkerExeOutput linker_exe_output_init(void) {
    return (LinkerExeOutput) {
        .arena = arena_alloc(),
        .section_trie = trie_init(),
        .global_table = init_linking_table(100),
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
    ret->next = ret;
    ret->prev = ret;
    return ret;
}

LinkerObject *linker_exe_add_object(LinkerExeOutput *output) {
    LinkerObject *new = linker_exe_output_alloc_obj(output);
    if (output->cursor == NULL) { // append to the end of the topmost layer
        if (output->root == NULL) {
            output->root = new;
        } else {
            linker_object_add_neighbor(output->root->prev, new);
        }
    } else {
        linker_object_add_child(output->cursor, new);
    }
    return new;
}

void linker_exe_start_segment(LinkerExeOutput *output, U64 type, U64 flags) {
    output->cursor = linker_exe_add_object(output);
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

void linker_exe_start_section(LinkerExeOutput *output, String8 name, U64 type, U64 flags) { // TODO: add Error handling
    output->cursor = linker_exe_add_object(output);
    output->cursor->type = LinkerObjType_OUTPUT_SECTION,
    output->cursor->obj.output_section =
        (LinkerOutputSection) {
            .name = name,
            .flags = flags,
        };
}

void linker_exe_end_section(LinkerExeOutput *output) {
    if (output->cursor == NULL) {
        Assert(0);
    }
    if (output->cursor->type != LinkerObjType_OUTPUT_SECTION) {
        Assert(0);
    }
    output->cursor = output->cursor->parent;
}

void linker_exe_add_section(LinkerExeOutput *output, String8 name, U64 type, U64 flags, TrieMatch match) {
    LinkerObject *node = linker_exe_add_object(output);
    node->type = LinkerObjType_INPUT_SECTION;
    node->obj.input_section = (LinkerInputSection) {
        .name = name,
        .type = type,
        .flags = flags,
    };
    trie_insert(&output->section_trie, name, node, match);
}

void linker_exe_add_alignment(LinkerExeOutput *output, U64 align) {
    LinkerObject *node = linker_exe_add_object(output);
    node->type = LinkerObjType_ALIGNMENT;
    node->obj.align = (LinkerAlignment) {
        .alignment = align,
    };
}

// special segments

void linker_exe_start_relro(LinkerExeOutput *output) {
    if (output->cursor == NULL) {
        Assert(0);
    }
    if (output->cursor->type != LinkerObjType_SEGMENT) {
        Assert(0);
    }
    // output->cursor->obj.segment.flags != (ELF_PFlag_Read | ELF_PFlag_Write) ||
    if (output->cursor->obj.segment.type != ELF_PType_Load) {
            Assert(0);
    }
    linker_exe_start_segment(output, ELF_PType_GnuRelro, ELF_PFlag_Read);
}

void linker_exe_start_tls(LinkerExeOutput *output) {
    if (output->cursor == NULL) {
        Assert(0);
    }
    if (output->cursor->type != LinkerObjType_SEGMENT) {
        Assert(0);
    }
    linker_exe_start_segment(output, ELF_PType_Tls, ELF_PFlag_Read);
}

void linker_exe_add_padding(LinkerExeOutput *output, U64 padding) {
    LinkerObject *node = linker_exe_add_object(output);
    node->type = LinkerObjType_PADDING;
    node->obj.pad = (LinkerPadding) {
        .padding = padding,
    };
}

void linker_exe_add_header(LinkerExeOutput *output){
    // check to see if this is the first possible memory location (required by standard)
    if (output->cursor == NULL) {
        Assert(0);
    }
    LinkerObject *parent = output->cursor;
    while (parent->parent != NULL) parent = parent->parent;
    if (parent != output->root) {
        Assert(0);
    }
    linker_exe_add_padding(output, sizeof(ELF_Hdr64));
}

void linker_exe_add_pheader(LinkerExeOutput *output){
    if (output->cursor == NULL) {
        Assert(0);
    }
    LinkerObject *node = linker_exe_add_object(output);
    node->type = LinkerObjType_PADDING;
    node->obj.pad = (LinkerPadding) {
        .padding = 0,
    };
    output->phdr_node = &node->obj.pad;
}

/*
test = linker_exe_init(void)
linker_exe_start_segment(alloc, readable, executable)
linker_exe_add_headers()
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

void load_input_sections (LinkerExeOutput *output, ElfFile *file) {
    for EachIndex(i, file->shdrs.count) {
        ElfSection *section = &file->shdrs.v[i];
        if (!(file->shdrs.v[i].hdr.sh_flags & ELF_Shf_Alloc)) { // limited exe only needs Alloc. sections <-- possibly add header data to it's own page for simplicity
            continue;
        }
        String8 section_name = get_section_name(file, &section->hdr);
        TrieNode *trie_node = trie_lookup(&output->section_trie, section_name);
        if (trie_node == NULL) {
            Assert(0); // TODO implement default location for segments
        }
        LinkerObject *node = trie_node->value;
        if (node->type != LinkerObjType_INPUT_SECTION) {
            Assert(0);
        }
        LinkerInputSection *input_section = &node->obj.input_section;
        if (input_section->flags != section->hdr.sh_flags || input_section->type!= section->hdr.sh_type) {
            continue;
        }
        if (input_section->head == NULL) {
            input_section->head = push_item(output->arena, ElfSection_list);
            input_section->tail = input_section->head;
        } else {
            input_section->tail->next = push_item(output->arena, ElfSection_list);
            input_section->tail = input_section->tail->next;
        }
        input_section->tail->section = section;
        input_section->tail->file = file;
    }
}

void load_input_files(LinkerExeOutput *output, ElfFile_array files) {
    output->input_files = files;
    for EachIndex(i, files.count) {
        load_input_sections(output, &files.v[i]);
    }
}

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

void linker_tree_process_leaf(LinkerExeOutput *output, LinkerObject *obj, LinkerPositionState *state) {
    switch (obj->type) {
        case LinkerObjType_ALIGNMENT: {
            state->offset = AlignPow2(state->offset, obj->obj.align.alignment);
            state->vaddr = AlignPow2(state->vaddr, obj->obj.align.alignment);
        }break;
        case LinkerObjType_INPUT_SECTION: {
            LinkerInputSection *section = &obj->obj.input_section;
            obj->offset = state->offset;
            obj->vaddr = state->vaddr;
            obj->mem_size = 0;
            obj->file_size = 0;
            for EachNode(it, ElfSection_list, section->head) {
                if (section->flags != it->section->hdr.sh_flags) {
                    Assert(0);
                }
                if(section->type != it->section->hdr.sh_type) {
                    Assert(0);
                }
                state->vaddr = AlignPow2(state->vaddr, it->section->hdr.sh_addralign);
                it->section->file_offset = state->offset;
                it->section->memory_offset = state->vaddr;
                obj->mem_size = (state->vaddr - obj->vaddr) + it->section->hdr.sh_size;
                state->vaddr += it->section->hdr.sh_size;
                if (section->type != ELF_ShType_NoBits) {
                    state->offset = AlignPow2(state->offset, it->section->hdr.sh_addralign);
                    it->section->file_offset = state->offset;
                    obj->file_size = (state->offset - obj->offset) + it->section->hdr.sh_size;
                    state->offset += it->section->hdr.sh_size;
                }
                for EachIndex(k, it->file->syms.count) {
                    // load symbol tables
                    if (ELF_ST_BIND(it->file->syms.v[k].st_info) == ELF_SymBind_Global ) {
                        add_symbol(output->arena, &output->global_table, it->file, &it->file->syms.v[k]);
                    }
                }
            }
        }break;
        case LinkerObjType_PADDING: {
            state->offset += obj->obj.pad.padding;
            obj->offset +=  obj->obj.pad.padding;
            state->vaddr +=  obj->obj.pad.padding;
            obj->vaddr +=  obj->obj.pad.padding;
            obj->file_size +=  obj->obj.pad.padding;
            obj->mem_size +=  obj->obj.pad.padding;
        }
        default: break;
    }
}

LinkerSizeResult compute_positions_linker_tree(LinkerExeOutput *output, LinkerObject *obj, LinkerPositionState *state) {
    LinkerObject *first = obj;
    LinkerSizeResult total = { 0 };
    do {
        LinkerSizeResult node_size = { 0 };
        if (obj->child != NULL) { // leaf node
            obj->vaddr = state->vaddr;
            obj->offset = state->offset;
            node_size = compute_positions_linker_tree(output, obj->child, state);
            obj->mem_size = node_size.mem_size;
            obj->file_size = node_size.file_size;
        } else {
            linker_tree_process_leaf(output, obj, state);
            node_size.mem_size += obj->mem_size;
            node_size.file_size += obj->file_size;
        }
        total.mem_size += node_size.mem_size;
        total.file_size += node_size.file_size;
        obj = obj->next;
    } while(obj != first);
    return total;
}


void linker_position_sections(LinkerExeOutput *output) {
    if (output->phdr_count == 0) {
        // odd for an executable to not define output

    }
    if (output->phdr_node != NULL) {
        output->phdr_node->padding = sizeof(ELF_Phdr64) * output->phdr_count;
    }
    LinkerPositionState state = (LinkerPositionState) {
        .offset = 0,
        .vaddr = 0x400000,
    };
    compute_positions_linker_tree(output, output->root, &state);
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

void linker_exe_process_node(OS_Handle output_file, LinkerExeOutput *output ,LinkerObject *obj, ELF_Phdr64Array *phdrs, U64 *root_idx, U64 *sub_idx) {
    LinkerObject *first = obj;
    if (obj == NULL) {
        return;
    }
    do {
        if (obj->type == LinkerObjType_SEGMENT) {
            LinkerSegment *seg = &obj->obj.segment;
            ELF_Phdr64 *phdr = NULL;
            if (obj->parent == NULL) {
                phdr = &phdrs->v[*root_idx];
                (*root_idx)++;
            } else {
                phdr = &phdrs->v[*sub_idx];
                (*sub_idx)--;
            }
            phdr->p_align = 0x1000; // bad, not all sections have this alignment
            phdr->p_flags = seg->flags;
            phdr->p_type = seg->type;
            phdr->p_filesz = obj->file_size;
            phdr->p_memsz = obj->mem_size;
            phdr->p_vaddr = obj->vaddr;
            phdr->p_paddr = obj->vaddr;
            phdr->p_offset = obj->offset;
            linker_exe_process_node(output_file, output, obj->child, phdrs, root_idx, sub_idx);
        } else if (obj->type == LinkerObjType_INPUT_SECTION) {
            ElfSection_list *sections = obj->obj.input_section.head;
            for EachNode(section, ElfSection_list, sections) {

                ElfSection *sec_header = section->section;
                ElfFile *file = section->file;
                if (sec_header->hdr.sh_type == ELF_ShType_NoBits) continue;
                Temp temp = temp_begin(output->arena);
                String8 data = load_section_data(temp.arena, file, &sec_header->hdr);
                // apply relocations
                apply_all_relocations(&output->global_table, file, sec_header, data);
                os_file_write(output_file, rng_1u64(sec_header->file_offset, sec_header->file_offset + data.size), data.str);
                temp_end(temp);
            }

        } else {
            linker_exe_process_node(output_file, output, obj->child, phdrs, root_idx, sub_idx);
        }
        obj = obj->next;
    } while (obj != first);
}

ELF_Phdr64Array linker_exe_build(OS_Handle output_file, LinkerExeOutput *output) {
    ELF_Phdr64Array phdrs = (ELF_Phdr64Array) {
        .v = push_array(output->arena, ELF_Phdr64, output->phdr_count),
        .count = output->phdr_count,
    };
    U64 root_header_idx = 0;
    U64 sub_header_idx = output->phdr_count-1;
    linker_exe_process_node(output_file, output, output->root, &phdrs, &root_header_idx, &sub_header_idx);
    return phdrs;
}

internal void output_elf_exe( LinkerExeOutput *output, String8 output_filename) {
    // write header and segment data
    OS_Handle output_file = os_file_open(OS_AccessFlag_Write, output_filename);

    Temp temp = temp_begin(output->arena);


    ELF_Hdr64 *hdr = &output->header;
    MemoryCopyStruct(hdr, &default_header);

    hdr->e_phoff = sizeof(ELF_Hdr64);
    hdr->e_phnum = output->phdr_count;
    U64 entry_address = 0;
    {
        SymbolNode *entry_symbol = get_symbol(&output->global_table, str8_lit("_start"));
        if (entry_symbol != NULL) {
            U64 st_offset = entry_symbol->file->shdrs.v[entry_symbol->sym->st_shndx].memory_offset;
            entry_address = entry_symbol->sym->st_value + st_offset;
        } else {
            // report error
        }
    }
    hdr->e_entry = entry_address;
    hdr->e_type = ELF_Type_Exec;
    Rng1U64 rng = rng_1u64(0, sizeof(ELF_Hdr64));
    os_file_write(output_file, rng, hdr);
    temp_end(temp);
    // load program headers
    ELF_Phdr64Array phdrs = linker_exe_build(output_file, output);
    rng = rng_1u64(rng.max, rng.max + sizeof(ELF_Phdr64) * phdrs.count);
    os_file_write(output_file, rng, phdrs.v);
    // load file section data
}
