#include <elf/elf.h>
#include <base/base_core.h>
#include "base/base_arena.h"

typedef struct Linker_Object {
    U8 *text;
    U64 text_size;
    U8 *rodata;
    U64 rodata_size;
    U8 *data;
    U64 data_size;
    U64 bss_size;

} Linker_Object;

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
        [ELF_Identifier_OsAbi] = ELF_OsAbi_SYSV,
    }, 
    .e_type = ELF_Type_None, 
    .e_machine = ELF_MachineKind_X86_64,
    .e_version = ELF_Version_Current, 
    .e_entry = 0, 
    .e_phoff = 0, 
    .e_shoff = 0,
    .e_flags = 0, 
    .e_ehsize = sizeof(ELF_Hdr64),
    .e_phentsize = sizeof(ELF_Phdr64) / sizeof(U64),
    .e_phnum = 0,
    .e_shentsize = sizeof(ELF_Shdr64) / sizeof(U64), 
    .e_shnum = 0,
    .e_shstrndx = ELF_SectionIndex_Undef,
};

typedef struct Linker_ElfFile {
    Arena *arena;
    Linker_Object *object;
    ELF_Hdr64 *hdr;
} Linker_elf_File;

generate_elf_executable(Arena *arena, Linker_Object)

    int main(void) {
}
