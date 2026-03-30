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
    OutputElfExe exe = buildOutputElfFile(arena, &files_array, 0x1000, 0x400000);
    build_elf_exe(arena, &exe, str8_lit("elf.out"));

    log_select(NULL); // deselect log before free
    log_release(main_log);
    arena_release(arena); // release arena
}
