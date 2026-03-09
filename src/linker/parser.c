#include <linker/parser.h>


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
