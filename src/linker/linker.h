#include <elf/elf.h>

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
