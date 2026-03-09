#ifndef LINKER_PARSE_H
#define LINKER_PARSE_H

#include <elf/elf.h>
#include <elf/elf_parse.h>
#include <base/base_inc.h>
#include <os/os_inc.h>

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
} ElfFile;

internal ElfFile read_elf_file(Arena *arena, String8 filename);

String8 get_section_name(ElfFile *file, ELF_Shdr64 *shdr);

String8 get_symbol_name(ElfFile *file, ELF_Sym64 *sym);

String8 get_reloc_name(ElfFile *file, ELF_Rela64 *rela);

#endif // LINKER_PARSE_H
