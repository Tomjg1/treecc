#ifndef LINKER_EXECUTABLE_GEN_H
#define LINKER_EXECUTABLE_GEN_H

#include <linker/parser.h>
#include <linker/symbol_lookup.h>
#include <base/base_inc.h>
#include <os/os_inc.h>

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

typedef struct OutputSections { // group by name
    String8 section_name;
    ElfSection_list *sections;
    struct OutputSections *next;
} OutputSections;

typedef struct OutputSegment { // group by flag/type
    ELF_Phdr64 phdr;
    U64 flags;
    OutputSections *prog_sections;  // output section is chosen by the type
    OutputSections *no_bit_sections;
    OutputSections *note_sections;
} OutputSegment;

typedef struct OutputElfExe {
    SymbolTable global_table;
    ElfFile_array input;
    OutputSegment output[5];
    U8 non_exec_stack;

    U64 segment_count;
} OutputElfExe;

void load_input_sections(Arena *arena, ElfFile *file, OutputElfExe *out_file);

internal OutputElfExe buildOutputElfFile(Arena *arena, ElfFile_array *array, U64 section_alignment, U64 base_image_address);

String8 load_section_data(Arena *arena, ElfFile *file, ELF_Shdr64 *shdr);


void apply_all_relocations(SymbolTable *global_table, ElfFile *file, ElfSection *shdr, String8 sec_data);

internal void build_elf_exe(Arena *arena, OutputElfExe *output,
                                   String8 output_filename);

#endif // LINKER_EXECUTABLE_GEN_H
