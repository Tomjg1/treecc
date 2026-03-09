#include <base/base_inc.h>
#include <os/os_inc.h>

internal no_inline void entry_point(CmdLine *cmdline) {
    OS_Handle file = os_file_open(OS_AccessFlag_Write, str8_lit("test.txt"));
    String8 test = str8_lit("test text");
    Rng1U64 rng = rng_1u64(0, test.size);
    os_file_write(file, rng, test.str);
    rng = rng_1u64(rng.max + 50, rng.max + 50 + test.size);
    os_file_write(file, rng, test.str);
}

#include <base/base_inc.c>
#include <os/os_inc.c>
