#include <stdio.h>
#define BUILD_ENTRY_DEFINING_UNIT 1

#include <base/base_inc.h>
#include <elf/elf.h>
#include <os/os_inc.h>
#include <elf/elf_parse.h>
#include <linker/linker_error.h>

typedef struct ElfSym64_Array {
    ELF_Sym64 *v;
    U64 count;
    String8 symstrs_data;
} ElfSym64_Array;

typedef struct ElfRela64_Array {
    ELF_Rela64 *v;
    U64 count;
} ElfRela64_Array;

typedef struct ElfRela64_Node ElfRela64_Node;
struct ElfRela64_Node {
    ElfRela64_Array relas;
    ElfRela64_Node *next;
};

typedef struct ElfSection {
    ELF_Shdr64 hdr;
    U64 file_offset;
    U64 memory_offset;
    ElfRela64_Node *relas;
} ElfSection;


typedef struct ElfSection_Array {
    ElfSection *v;
    U64 count;
    String8 shstrs_data;
} ElfSection_Array;

typedef struct ElfFile {
    OS_Handle handle;
    String8 fileName;
    ELF_Hdr64 hdr;
    ElfSection_Array shdrs;
    ElfSym64_Array syms;
    //ElfReloc64_Node *relocs;
} ElfFile;

internal static U64 read_section(OS_Handle file, ELF_Shdr64 *shdr,
                                 void *out_data) {
    return os_file_read(
        file, rng_1u64(shdr->sh_offset, shdr->sh_offset + shdr->sh_size),
        out_data);
}

internal ElfFile read_elf_file(Arena *arena, String8 filename) {
    ElfFile file = { 0 };
    // TG: read elf header
    {
        file.handle = os_file_open(OS_AccessFlag_Read, filename);
        if (os_handle_match(file.handle, os_handle_zero()))
            return file;
        file.fileName = filename;

        U8 elf_ident[ELF_Identifier_Max] = { 0 };

        U64 bytes_read = os_file_read(
            file.handle, rng_1u64(0, sizeof(elf_ident)), elf_ident);

        String8 file_magic_number = {
            .str = elf_ident,
            .size = elf_magic_string.size,
        };
        if (bytes_read == ELF_Identifier_Max &&
            str8_match(file_magic_number, elf_magic_string, 0) &&
            elf_ident[ELF_Identifier_Class] == ELF_Class_64 &&
            elf_ident[ELF_Identifier_OsAbi] == 0 &&
            elf_ident[ELF_Identifier_Data] == ELF_Data_2LSB) {
            ELF_Hdr64 hdr = { 0 };
            bytes_read = os_file_read_struct(file.handle, 0, &hdr);

            if (bytes_read == sizeof(hdr)) {
                file.hdr = hdr;
            }
        }
    }
    // TG: read elf section header
    {
        ELF_Hdr64 *hdr = &file.hdr;

        if (hdr->e_shentsize == sizeof(ELF_Shdr64)) {
            // read the sections
            ElfSection_Array shdrs_offset = { 0 };
            shdrs_offset.count = hdr->e_shnum;
            shdrs_offset.v = push_array(arena, ElfSection, shdrs_offset.count);

            Temp temp = temp_begin(arena);
            ELF_Shdr64Array shdrs = { 0 };
            shdrs.count = hdr->e_shnum;
            shdrs.v = push_array(temp.arena, ELF_Shdr64, shdrs.count);

            U64 bytes_read = os_file_read(
                file.handle,
                r1u64(hdr->e_shoff,
                      hdr->e_shoff + shdrs.count * sizeof(*shdrs.v)),
                shdrs.v);

            if (shdrs.count > 0 &&
                bytes_read == shdrs.count * sizeof(*shdrs.v)) {
                for
                    EachIndex(i, shdrs.count) {
                        shdrs_offset.v[i].hdr = shdrs.v[i];
                    }
                file.shdrs = shdrs_offset;
                temp_end(temp);
                U64 shstrs_idx;
                if (hdr->e_shstrndx == ELF_SectionIndex_HiReserve) {
                    shstrs_idx = file.shdrs.v[0].hdr.sh_link;
                } else {
                    shstrs_idx = hdr->e_shstrndx;
                }
                if (shstrs_idx < file.shdrs.count) {
                    String8 shstrs_data = { 0 };
                    shstrs_data.size = file.shdrs.v[shstrs_idx].hdr.sh_size;
                    shstrs_data.str = push_array(arena, U8, shstrs_data.size);
                    bytes_read = read_section(file.handle,
                                              &file.shdrs.v[shstrs_idx].hdr,
                                              shstrs_data.str);
                    if (bytes_read == shstrs_data.size) {
                        file.shdrs.shstrs_data = shstrs_data;
                    }
                }
            } else {
                temp_end(temp);
            }
        }
    }
    // TG: read symbol table
    {
        ElfSection_Array *shdrs = &file.shdrs;
        for
            EachIndex(i, shdrs->count) {
                if (shdrs->v[i].hdr.sh_type == ELF_ShType_Symtab) {
                    ElfSym64_Array syms = { 0 };
                    syms.count =
                        shdrs->v[i].hdr.sh_size / shdrs->v[i].hdr.sh_entsize;
                    syms.v = push_array(arena, ELF_Sym64, syms.count);
                    U64 bytes_read =
                        read_section(file.handle, &shdrs->v[i].hdr, syms.v);

                    // read associated sym table
                    if (bytes_read == shdrs->v[i].hdr.sh_size) {
                        U64 symstrs_idx = shdrs->v[i].hdr.sh_link;
                        String8 symstrs_data = { 0 };
                        if (symstrs_idx < shdrs->count) {
                            symstrs_data.size =
                                shdrs->v[symstrs_idx].hdr.sh_size;
                            symstrs_data.str =
                                push_array(arena, U8, symstrs_data.size);
                            bytes_read = read_section(
                                file.handle, &shdrs->v[symstrs_idx].hdr,
                                symstrs_data.str);
                            if (bytes_read == symstrs_data.size) {
                                syms.symstrs_data = symstrs_data;
                                file.syms = syms;
                            }
                        }
                    }
                    break; // TG: assume that there is only one -> change if the standard ever changes
                }
            }
    }
    // TG: read relocatable sections
    {
        ElfSection_Array *shdrs = &file.shdrs;
        ElfRela64_Array relas = {0};
        for EachIndex(i, shdrs->count) {
            if (shdrs->v[i].hdr.sh_type == ELF_ShType_Rela) {
                U64 sec_idx = shdrs->v[i].hdr.sh_info;
                ElfRela64_Node *new_node =
                    push_item(arena, ElfRela64_Node);
                new_node->relas.count =
                    shdrs->v[i].hdr.sh_size / shdrs->v[i].hdr.sh_entsize;
                new_node->relas.v =
                    push_array(arena, ELF_Rela64, new_node->relas.count);
                U64 bytes_read = read_section(file.handle, &shdrs->v[i].hdr,
                                            new_node->relas.v);
                if (bytes_read ==
                    new_node->relas.count * sizeof(*new_node->relas.v) && sec_idx < shdrs->count){
                        SLLStackPush(shdrs->v[sec_idx].relas, new_node);
                }
            }
        }
    }
    return file;
}

String8 get_section_name(ElfFile *file, ELF_Shdr64 *shdr) {
    String8 name = { 0 };
    str8_deserial_read_cstr(file->shdrs.shstrs_data, shdr->sh_name, &name);
    return name;
}

String8 get_symbol_name(ElfFile *file, ELF_Sym64 *sym) {
    String8 name = { 0 };
    if (ELF_ST_TYPE(sym->st_info) == ELF_SymType_Section) {
        name = get_section_name(file, &file->shdrs.v[sym->st_shndx].hdr);
    }
    str8_deserial_read_cstr(file->syms.symstrs_data, sym->st_name, &name);
    return name;
}
String8 get_reloc_name(ElfFile *file, ELF_Rela64 *rela) {
    U64 sym_idx = ELF64_R_SYM(rela->r_info);
    return get_symbol_name(file, &file->syms.v[sym_idx]);
}

typedef struct LocationNode LocationNode;
struct LocationNode {
    ElfFile *file;
    ELF_Rela64 *rela;
    LocationNode *next;
};

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

U64 new_hash(String8 name) {
    uint32_t h = 5381;
    for
        EachIndex(i, name.size) {
            h = h * 33 + name.str[i];
        }
    return h;
}

LinkingTable init_linking_table(void) {
    LinkingTable table = { 0 };
    table.arena = arena_alloc();
    table.count = 50;
    table.lookup = push_array(table.arena, SymbolNode *, table.count);
    return table;
}

internal SymbolNode *get_symbol(LinkingTable *table, String8 sym_name) {
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

internal LinkerError add_symbol(Arena *arena, LinkingTable *table,
                                ElfFile *file, ELF_Sym64 *sym) {
    if (table->count == 0) {
        return LINKER_ERROR_GENERIC;
    }

    String8 sym_name = get_symbol_name(file, sym);
    SymbolNode *bucket = get_symbol(table, sym_name);
    if (bucket != NULL) {
        if (sym->st_shndx == 0) {
            // symbol already exists, no need to make an undefined symbol
            return LINKER_ERROR_NONE;
        }
        if (bucket->sym->st_shndx != 0) {
            // symbol already defined
            return LINKER_ERROR_SYM_DEFINED;
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

void deinit_linking_table(LinkingTable *table) {
    arena_release(table->arena);
}

typedef struct ElfFile_array {
    ElfFile *v;
    U64 count;
} ElfFile_array;

typedef struct ElfSection_list {
    ElfSection *section;
    ElfFile *file;
    struct ElfSection_list *next;
} ElfSection_list;

typedef struct ElfSegment{
    ELF_Phdr64 phdr;
    struct ElfSegment *next;
} ElfSegment;

typedef struct ElfSegment_table{
    ElfSegment *phdrs;
    U64 count;
} ElfSegment_table;

U32 segment_flags_from_section(U64 sh_flags) {
    U32 flags = 0;
    if (sh_flags & ELF_Shf_Alloc) flags |= ELF_PFlag_Read;
    if (sh_flags & ELF_Shf_ExecInstr) flags |= ELF_PFlag_Exec;
    if (sh_flags & ELF_Shf_Write) flags |= ELF_PFlag_Write;
    return flags;
}
typedef struct OutputSections { // group by name
    String8 section_name;
    ElfSection_list *sections;
    struct OutputSections *next;
} OutputSections;

typedef struct OutputSegment { // group by flag/type
    ELF_Phdr64 phdr;
    U64 flags;
    OutputSections *sections;
    OutputSections *no_bit_sections;
} OutputSegment;

typedef struct OutputElfExe {
    LinkingTable global_table;
    ElfFile_array input;
    OutputSegment output[3];
    U64 segment_count;
} OutputElfExe;

static U8 section_category(U64 flags, U64 type) {
    U8 out;
    if (!(flags & ELF_Shf_Alloc)) {
        Assert(0); // non Allocatables cannot be included.
    } else if (flags & ELF_Shf_ExecInstr) {
        out = 0; // RX
    } else if (flags & ELF_Shf_Write) {
        out = 2; // RW
    } else {
        out = 1; // R
    }
    return out;
}

void load_input_sections(Arena *arena, ElfFile *file, OutputElfExe *out_file) {
    for EachIndex(i, file->shdrs.count) {
        if (!(file->shdrs.v[i].hdr.sh_flags & ELF_Shf_Alloc)) { // limited exe only needs Alloc. sections
            continue;
        }
        /*if (file->shdrs.v[i].hdr.sh_type == ELF_ShType_Note) {
            out_file->segment_count++;
            }*/
        String8 sec_name = get_section_name(file, &file->shdrs.v[i].hdr);
        U64 sec_flags = file->shdrs.v[i].hdr.sh_flags;
        U64 sec_type = file->shdrs.v[i].hdr.sh_type;

        OutputSections **it = NULL;
        out_file->output[section_category(sec_flags, sec_type)].flags = sec_flags;
        if (sec_type == ELF_ShType_NoBits) {
            it = &out_file->output[section_category(sec_flags, sec_type)].no_bit_sections;
        } else {
            it = &out_file->output[section_category(sec_flags, sec_type)].sections;
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
        output.global_table = init_linking_table();
        U64 current_mem_base = base_image_address; //
        U64 current_file_base = 0; //
        U64 current_file_offset =  sizeof(ELF_Hdr64) + sizeof(ELF_Phdr64) * output.segment_count; //reserve space for headers
        U64 current_mem_offset =  current_file_offset + base_image_address;
        for EachIndex(i, ArrayCount(output.output)) {
            OutputSegment *segment = &output.output[i];
            if (segment->sections != NULL) {
                for EachNode(j, OutputSections, segment->sections) {
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
                for EachNode(j, OutputSections, segment->sections) {
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


void apply_all_relocations(LinkingTable *global_table, ElfFile *file, ElfSection *shdr, String8 sec_data){

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

/*ELF_Phdr64 build_segment_header(Arena *arena, OutputSegment *segment, U64 base_image_address) {
    ELF_Phdr64 phdr = {0};
    for EachNode(section_list, OutputSections, segment->sections) {
        for EachNode(ot, ElfSection_list, section_list->sections) {
            ELF_Shdr64 *sh = &ot->section->hdr;

            if (!(sh->sh_flags & ELF_Shf_Alloc)) continue;

            U32 flags = segment_flags_from_section(sh);

            U64 sec_offset = ot->section->file_offset;
            U64 sec_vaddr = base_image_address + ot->section->memory_offset;

            U64 filesz = (sh->sh_type == ELF_ShType_NoBits) ? 0 : sh->sh_size;
            U64 memsz = sh->sh_size;


            phdr.p_type = ELF_PType_Load;
            phdr.p_flags = flags;
            phdr.p_offset = sec_offset;
            phdr.p_vaddr = sec_vaddr;
            phdr.p_paddr = sec_vaddr;
            phdr.p_filesz = filesz;
            phdr.p_memsz = memsz;
            phdr.p_align = 0x1000;
        }
    }
    return phdr;
}

internal ElfSegment_table generate_program_header(Arena *arena, OutputElfExe *file, U64 base_image_address) {
    ElfSegment_table table = {0};
    ElfSegment *current_node = NULL;

    if ((base_image_address & 0xFFF) != 0) {
        return table;
    }

    for EachIndex(section_list, ArrayCount(file->output)) {
        ElfSection_list section
        for EachNode(ot, ElfSection_list, section->sections) {
            ELF_Shdr64 *sh = &ot->section->hdr;

            if (!(sh->sh_flags & ELF_Shf_Alloc)) continue;

            U32 flags = segment_flags_from_section(sh);

            U64 sec_offset = ot->section->file_offset;
            U64 sec_vaddr = base_image_address + ot->section->memory_offset;

            U64 filesz = (sh->sh_type == ELF_ShType_NoBits) ? 0 : sh->sh_size;
            U64 memsz = sh->sh_size;


            if (current_node != NULL && current_node->phdr.p_flags == flags) {
                current_node->phdr.p_filesz += filesz;
                current_node->phdr.p_memsz  += memsz;
            } else {
                ElfSegment *new_node = push_item(arena, ElfSegment);
                if (table.phdrs == NULL) {
                    table.phdrs = new_node;
                } else {
                    current_node->next = new_node;
                }
                current_node = new_node;
                table.count++;
                current_node->phdr.p_type = ELF_PType_Load;
                current_node->phdr.p_flags = flags;
                current_node->phdr.p_offset = sec_offset;
                current_node->phdr.p_vaddr = sec_vaddr;
                current_node->phdr.p_paddr = sec_vaddr;
                current_node->phdr.p_filesz = filesz;
                current_node->phdr.p_memsz = memsz;
                current_node->phdr.p_align = 0x1000;
            }
        }
    }
    return table;
}*/

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

        for EachNode(it, OutputSections, segments.sections) {
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


internal no_inline void entry_point(CmdLine *cmdline) {
    Arena *arena = arena_alloc();
    ElfFile files[2] = {
         read_elf_file(arena, str8_lit("./reference_elf/undef.o")),
         read_elf_file(arena, str8_lit("./reference_elf/def.o"))
    };
    ElfFile_array files_array = {
        .v = files,
        .count = ArrayCount(files),
    };
    OutputElfExe exe = buildOutputElfFile(arena, &files_array, 0x1000, 0x400000);
    build_elf_exe(arena, &exe, str8_lit("elf.out"));
}

#include <base/base_inc.c>
#include <os/os_inc.c>
#include <elf/elf_parse.c>
#include <elf/elf.c>
