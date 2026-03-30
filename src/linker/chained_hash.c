#include "executable_gen.h"
#include "parser.h"
#include <base/base_inc.h>




load_input_section()

OutputSections gather_input_sections(ElfFile_array files);



typedef struct OutputExe {
    // loadable segments r - rx - r - rw
    // tls sections in rw
    // relro sections in rw
    //
} a;

generate_output_segments();

align_sections();
