#define BUILD_ENTRY_DEFINING_UNIT 1

#include <elf/elf.h>
#include <os/os_inc.h>

typedef enum Result {
    Result_OK = 0,
    Result_ERR = 1,
    Result_MIX = 2,
} Result;

typedef struct ElfFile {
    enum {
        ELF_FILE_STATE_OPEN,
        ELF_FILE_STATE_CLOSED,
    } file_state;
    OS_Handle file;
} ElfFile;

typedef struct open_elf_file_result {
    Result result;
    ElfFile ok;
    enum {
        ELF_FILE_ERROR_DNE,
        ELF_FILE_ERROR_INV,
    } err;
} open_elf_file_result;

/**
 * \brief opens an ELF file.
 */
open_elf_file_result open_elf_file(String8 filename) {
    open_elf_file_result out;
    out.ok = (ElfFile){
        .file = os_file_open(OS_AccessFlag_Read, filename),
        .file_state = ELF_FILE_STATE_OPEN,
    };
    if (os_handle_match(os_handle_zero(), out.ok.file)) {
        out.result = Result_ERR;
        out.err = ELF_FILE_ERROR_DNE;
        return out;
    }
    U8 magic_number[4] = { 0 };
    if (!os_file_read(out.ok.file, (Rng1U64){ 0, 3 }, magic_number) ||
        MemoryMatch(magic_number, elf_magic, sizeof(magic_number))) {
        out.result = Result_ERR;
        out.err = ELF_FILE_ERROR_INV;
        return out;
    }
    out.result = Result_OK;
    return out;
}

internal void read_elf_header(ELF_Hdr64 *hdr, ElfFile *file) {
}

internal no_inline void entry_point(CmdLine *cmdline) {
