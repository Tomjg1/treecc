#define BUILD_ENTRY_DEFINING_UNIT 1

#include <base/base_inc.h>
#include <elf/elf.h>
#include <os/os_inc.h>
#include <elf/elf_parse.h>
#include <linker/linker_error.h>
#include <linker/parser.h>
#include <linker/symbol_lookup.h>
#include <linker/executable_gen.h>
#include <base/base_log.h>

void print_offset(U64 tabs) {
    for (U64 i=0; i<tabs; i++) {
        printf("\t");
    }
}

void print_node_row(U64 tabs, LinkerObject *obj) {
    LinkerObject *first = obj;
    do {
        print_offset(tabs);
        printf("%s\n", linker_obj_type_get_name(obj->type));
        print_offset(tabs);
        printf("vaddr: %lx\n", obj->vaddr);
        print_offset(tabs);
        printf("offset: %lx\n", obj->offset);
        print_offset(tabs);
        printf("mem_size: %lx\n", obj->mem_size);
        print_offset(tabs);
        printf("file_size: %lx\n", obj->file_size);
        if (obj->child != NULL) {
            print_node_row(tabs+1, obj->child);
        }
        obj = obj->next;
    } while (obj != first);
}

void print_nodes(LinkerExeOutput *exe) {
    print_node_row(0, exe->root);
}


internal no_inline void entry_point(CmdLine *cmdline) {
    Log *main_log = log_alloc();
    log_select(main_log);

    Arena *arena = arena_alloc();

    ElfFile files[2] = {
         read_elf_file(arena, str8_lit("./reference_elf/undef.o")),
         read_elf_file(arena, str8_lit("./reference_elf/def.o"))
    };
    ElfFile_array files_array = {
        .v = files,
        .count = ArrayCount(files),
    };

    // testing trie

    Trie tree = trie_init();
    trie_insert(&tree, str8_lit("TEST"), NULL , TRIE_MATCH_WILDCARD);
    trie_insert(&tree, str8_lit("TORRENT"), NULL , TRIE_MATCH_WILDCARD);
    trie_insert(&tree, str8_lit("TROJAN"), NULL , TRIE_MATCH_WILDCARD);

    LinkerExeOutput exe = linker_exe_output_init();
    U64 page_size = 1UL << 12;
    linker_exe_start_segment(&exe, ELF_PType_Load, ELF_PFlag_Read);
    linker_exe_add_header(&exe);
    linker_exe_add_pheader(&exe);
    linker_exe_add_alignment(&exe, page_size);
    linker_exe_end_segment(&exe);
    linker_exe_start_segment(&exe, ELF_PType_Load, ELF_PFlag_Read | ELF_PFlag_Exec);
    linker_exe_add_section(&exe, str8_lit(".text"), ELF_ShType_ProgBits, ELF_Shf_Alloc | ELF_Shf_ExecInstr, TRIE_MATCH_WILDCARD);
    linker_exe_add_alignment(&exe, page_size);
    linker_exe_end_segment(&exe);
    linker_exe_start_segment(&exe, ELF_PType_Load, ELF_PFlag_Read | ELF_PFlag_Write);
    linker_exe_add_section(&exe, str8_lit(".data"), ELF_ShType_ProgBits, ELF_Shf_Alloc | ELF_Shf_Write, TRIE_MATCH_WILDCARD);
    linker_exe_add_section(&exe, str8_lit(".bss"), ELF_ShType_NoBits, ELF_Shf_Alloc | ELF_Shf_Write, TRIE_MATCH_WILDCARD);
    linker_exe_add_alignment(&exe, page_size);
    linker_exe_end_segment(&exe);
    linker_exe_start_segment(&exe, ELF_PType_Load, ELF_PFlag_Read);
    linker_exe_start_relro(&exe);
    linker_exe_start_tls(&exe);
    linker_exe_add_section(&exe, str8_lit(".tdata"), ELF_ShType_ProgBits, ELF_Shf_Alloc | ELF_Shf_Write | ELF_Shf_Tls, TRIE_MATCH_WILDCARD);
    linker_exe_add_section(&exe, str8_lit(".tbss"), ELF_ShType_NoBits, ELF_Shf_Alloc | ELF_Shf_Write | ELF_Shf_Tls, TRIE_MATCH_WILDCARD);
    linker_exe_end_tls(&exe);
    linker_exe_add_alignment(&exe, page_size);
    linker_exe_end_relro(&exe);
    linker_exe_add_section(&exe, str8_lit(".note.gnu"), ELF_ShType_Note, ELF_Shf_Alloc, TRIE_MATCH_WILDCARD);
    linker_exe_add_section(&exe, str8_lit(".eh_frame"), ELF_ShType_ProgBits, ELF_Shf_Alloc, TRIE_MATCH_WILDCARD);
    linker_exe_add_section(&exe, str8_lit(".rodata"), ELF_ShType_ProgBits, ELF_Shf_Alloc, TRIE_MATCH_WILDCARD);
    linker_exe_add_alignment(&exe, page_size);
    linker_exe_end_segment(&exe);

    load_input_files(&exe, files_array);

    linker_position_sections(&exe);

    print_nodes(&exe);
    output_elf_exe(&exe, str8_lit("test.elf"));

    log_select(NULL); // deselect log before free
    log_release(main_log);
    arena_release(arena); // release arena
}
