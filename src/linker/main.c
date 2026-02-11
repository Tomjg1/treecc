#define BUILD_ENTRY_DEFINING_UNIT 1

#include <elf/elf.h>
#include <os/os_inc.h>

#define X64_ELF_BASE_VADDR (4 << 20)

typedef struct LinkObject {
    U8 *text;
    U64 text_size;
    U8 *rodata;
    U64 rodata_size;
    U8 *data;
    U64 data_size;
    U64 bss_size;
    U64 buffer_size;
} LinkObject;

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

typedef struct LinkElfFile {
    Arena *arena;
    LinkObject *object;
    ELF_Hdr64 *hdr;
    ELF_Phdr64 *phdr;
} LinkElfFile;

#define TEXT_SEGMENT 0
#define RODATA_SEGMENT 1
#define DATA_SEGMENT 2
#define GNU_STACK_SEGMENT 3

static inline U64 addPadding(U64 offset, U64 align) {
    return (offset + (align - 1)) & -align;
}

LinkElfFile init_elf(Arena *arena, LinkObject *object) {
    U64 pageSz = os_get_system_info()->page_size;

    LinkElfFile output = (LinkElfFile){
        .arena = arena,
        .hdr = push_item_no_zero(arena, ELF_Hdr64),

    };

    MemoryCopyStruct(output.hdr, &default_header);
    const U64 pHeadNum = 4;

    output.hdr->e_type = ELF_Type_Exec;
    output.hdr->e_entry = X64_ELF_BASE_VADDR + 0x224;

    ELF_Phdr64 *pHead = push_array(arena, ELF_Phdr64, pHeadNum);
    output.phdr = pHead;

    output.hdr->e_phoff = (U64)pHead - (U64)output.hdr;
    output.hdr->e_phnum = pHeadNum;

    pHead[TEXT_SEGMENT] = (ELF_Phdr64){
        .p_type = ELF_PType_Load,
        .p_offset = 0,
        .p_vaddr = X64_ELF_BASE_VADDR,
        .p_paddr = 0,
        .p_filesz = object->text_size,
        .p_memsz = object->text_size,
        .p_flags = ELF_PFlag_Exec | ELF_PFlag_Read,
        .p_align = pageSz,
    };

    U64 rodata_offset = addPadding(object->text_size, pageSz);
    pHead[RODATA_SEGMENT] = (ELF_Phdr64){
        .p_type = ELF_PType_Load,
        .p_offset = rodata_offset,
        .p_vaddr = X64_ELF_BASE_VADDR + rodata_offset,
        .p_paddr = rodata_offset,
        .p_filesz = object->rodata_size,
        .p_memsz = object->rodata_size,
        .p_flags = ELF_PFlag_Read,
        .p_align = pageSz,
    };

    U64 data_offset = addPadding(rodata_offset + object->rodata_size, pageSz);

    pHead[DATA_SEGMENT] = (ELF_Phdr64){
        .p_type = ELF_PType_Load,
        .p_offset = data_offset,
        .p_vaddr = X64_ELF_BASE_VADDR + data_offset,
        .p_paddr = data_offset,
        .p_filesz = object->data_size,
        .p_memsz = object->data_size,
        .p_flags = ELF_PFlag_Read | ELF_PFlag_Write,
        .p_align = pageSz,
    };

    pHead[GNU_STACK_SEGMENT] = (ELF_Phdr64){
        .p_type = ELF_PType_GnuStack,
        .p_offset = 0,
        .p_vaddr = 0,
        .p_paddr = 0,
        .p_filesz = 0,
        .p_memsz = 0,
        .p_flags = ELF_PFlag_Read | ELF_PFlag_Write,
        .p_align = 0x10,
    };

    return output;
}

void writeSegment(OS_Handle file, U64 base_offset, const ELF_Phdr64 *hdr,
                  U8 *data) {
    os_file_write(file,
                  (Rng1U64){ base_offset + hdr->p_offset,
                             base_offset + hdr->p_filesz },
                  data);
}

void generate_elf_executable(LinkObject *object, const String8 file_name) {
    LinkElfFile elf_format = init_elf(arena_alloc(), object);
    OS_Handle file = os_file_open(OS_AccessFlag_Write, file_name);

    U64 start_offset = sizeof(ELF_Hdr64);
    const ELF_Phdr64 *phdr = elf_format.phdr;
    os_file_write(file, (Rng1U64){ 0, sizeof(ELF_Hdr64) }, elf_format.hdr);

    const U64 base_offset =
        start_offset + sizeof(ELF_Phdr64) * elf_format.hdr->e_phnum;
    os_file_write(file, (Rng1U64){ start_offset, base_offset }, phdr);

    writeSegment(file, base_offset, &phdr[TEXT_SEGMENT], object->text);
    writeSegment(file, base_offset, &phdr[RODATA_SEGMENT], object->rodata);
    writeSegment(file, base_offset, &phdr[DATA_SEGMENT], object->data);
    os_file_close(file);
}

struct file_data {
    U64 size;
    U8 *data;
};

internal struct file_data getSegment(Arena *arena, String8 fileName) {
    OS_Handle file = os_file_open(OS_AccessFlag_Read, fileName);
    struct file_data out;
    out.size = os_properties_from_file(file).size;
    out.data = push_array(arena, U8, out.size);
    os_file_read(file, (Rng1U64){ 0, out.size }, out.data);
    os_file_close(file);
    return out;
}

internal no_inline void entry_point(CmdLine *cmdline) {
    Arena *arena = arena_alloc();
    struct file_data text = getSegment(arena, str8_lit("text.bin"));
    struct file_data rodata = getSegment(arena, str8_lit("rodata.bin"));
    struct file_data data = getSegment(arena, str8_lit("data.bin"));
    LinkObject test = (LinkObject){
        .text_size = text.size,
        .text = text.data,
        .data_size = data.size,
        .data = data.data,
        .rodata_size = rodata.size,
        .rodata = rodata.data,
    };
    generate_elf_executable(&test, str8_lit("tom.out"));
}

#include <base/base_inc.c>
#include <os/os_inc.c>
