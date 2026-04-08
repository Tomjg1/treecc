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
/*
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
*/

// Trie

typedef enum {
    TRIE_PREFIX_MATCH_MISMATCH = 0,
    TRIE_PREFIX_MATCH_SHORT,
    TRIE_PREFIX_MATCH_FULL,
    TRIE_PREFIX_MATCH_EQUAL,
} TriePrefixMatch;

typedef struct TriePrefixMatchResult{
    TriePrefixMatch type;
    U64 offset;
} TriePrefixMatchResult;

TriePrefixMatchResult trie_string8_match_prefix (String8 prefix, String8 other) {
    TriePrefixMatchResult res = { 0 };
    U64 len = Min(prefix.size, other.size);
    U64 i = 0;
    for (; i < len ;i++) {
        if (prefix.str[i] != other.str[i]) {
            res.type = TRIE_PREFIX_MATCH_MISMATCH;
            res.offset = i;
            return res;
        }
    }
    res.offset = i;
    if (other.size > prefix.size){
        res.type = TRIE_PREFIX_MATCH_FULL;
    } else if (other.size < prefix.size) {
        res.type = TRIE_PREFIX_MATCH_SHORT;
    } else {
        res.type = TRIE_PREFIX_MATCH_EQUAL;
    }

    return res;
}

typedef enum {
    TRIE_MATCH_WILDCARD,
    TRIE_MATCH_EXACT,
    TRIE_MATCH_NONE,
} TrieMatch;


typedef struct TrieNode {
    String8 prefix; // something like .text.
    void *value;
    TrieMatch type;
    struct TrieNode **lookup;
    struct TrieNode *neighbor;
    U64 count;
} TrieNode;

typedef struct Trie {
    TrieNode *root;
    Arena *arena;
} Trie;

typedef struct ElfSection_list {
    ElfSection *section; // ElfSection has final offsets for every section involved (points to input_files)
    ElfFile *file;
    struct ElfSection_list *next;
} ElfSection_list;

typedef struct LinkerInputSection {// OutputSections will be added to the trie for loading
    String8 name;
    U64 flags;
    U64 type;
    ElfSection_list *head;
    ElfSection_list *tail;
} LinkerInputSection;

typedef struct LinkerOutputSection {
    String8 name;
    U64 flags;
} LinkerOutputSection;

typedef struct LinkerSegment {
    U64 flags;
    U64 type;
} LinkerSegment;

typedef struct LinkerAlignment {
    U64 alignment;
} LinkerAlignment;

typedef struct LinkerSymbol {
    String8 name;
    U64 type;
    U64 size;

} LinkerSymbol;

typedef struct LinkerPadding {
    U64 padding;
} LinkerPadding;

typedef enum LinkerObjType{
    LinkerObjType_SEGMENT, // used to create program header
    LinkerObjType_OUTPUT_SECTION, // groups input sections into output sections
    LinkerObjType_INPUT_SECTION, // sections to be linked together
    LinkerObjType_ALIGNMENT, // ensures current offset is aligned
    LinkerObjType_SYMBOL, // create a symbol
    LinkerObjType_PADDING, // adds padding to offset + vaddr
    LinkerObjType_COUNT,
} LinkerObjType;

const char *LinkerObjType_name[] = {
    [LinkerObjType_SEGMENT] = Stringify(LinkerObjType_SEGMENT),
    [LinkerObjType_OUTPUT_SECTION] = Stringify(LinkerObjType_OUTPUT_SECTION),
    [LinkerObjType_INPUT_SECTION] = Stringify(LinkerObjType_INPUT_SECTION),
    [LinkerObjType_ALIGNMENT] = Stringify(LinkerObjType_ALIGNMENT),
    [LinkerObjType_SYMBOL] = Stringify(LinkerObjType_SYMBOL),
    [LinkerObjType_PADDING] = Stringify(LinkerObjType_PADDING),
    [LinkerObjType_COUNT] = Stringify(LinkerObjType_COUNT),
};

const char *linker_obj_type_get_name(LinkerObjType type) {
    return LinkerObjType_name[type];
}

typedef struct LinkerObject {
    LinkerObjType type;
    struct LinkerObject *parent;
    struct LinkerObject *next;
    struct LinkerObject *prev;
    struct LinkerObject *child;
    U64 offset;
    U64 vaddr;
    U64 file_size;
    U64 mem_size;
    U64 align;
    union {
        LinkerSegment segment;
        LinkerOutputSection output_section;
        LinkerInputSection input_section;
        LinkerAlignment align;
        LinkerSymbol symbol;
        LinkerPadding pad;
    } obj;
} LinkerObject;

typedef struct LinkerExeOutput {
    Arena *arena;
    ElfFile_array input_files;
    LinkerObject *root;
    LinkerObject *cursor;
    SymbolTable global_table;
    Trie section_trie;
    LinkerObject *freeList;
    ELF_Hdr64 header;
    // slightly odd way to handle the fact that
    // the first segment must contain both headers
    // phdrs will point to the object in tree so
    // it can be set after the tree is constructed
    LinkerPadding *phdr_node;
    U64 phdr_count;
    ELF_Phdr64Array pheaders;
} LinkerExeOutput;

typedef struct LinkerSegment_array {
    ELF_Phdr64 *v;
    U64 count;
} LinkerSegment_array;

typedef struct LinkerPositionState {
    U64 offset;
    U64 vaddr;
} LinkerPositionState;

typedef struct LinkerSizeResult {
    U64 mem_size;
    U64 file_size;
} LinkerSizeResult;

void load_input_sections(LinkerExeOutput *output, ElfFile *file);

//internal OutputElfExe buildOutputElfFile(Arena *arena, ElfFile_array *array, U64 section_alignment, U64 base_image_address);

String8 load_section_data(Arena *arena, ElfFile *file, ELF_Shdr64 *shdr);


void apply_all_relocations(SymbolTable *global_table, ElfFile *file, ElfSection *shdr, String8 sec_data);

//internal void build_elf_exe(Arena *arena, OutputElfExe *output, String8 output_filename);



Trie trie_init();

void trie_release(Trie *trie);

TrieNode *trie_node_lookup(TrieNode *node, char key);

TriePrefixMatchResult trie_prefix_search(TrieNode **node, String8 *key);


TrieNode *trie_node_alloc(Trie *trie, String8 key, void *value, TrieMatch match_type);

void trie_node_insert(Trie *trie, TrieNode *node, TrieNode *child);

TrieNode *trie_lookup(Trie *trie, String8 key);

TrieNode *trie_insert(Trie *trie, String8 key, void *value, TrieMatch match_type);

void linker_object_add_neighbor(LinkerObject *node, LinkerObject *neighbor);

void linker_object_add_child(LinkerObject *parent, LinkerObject *child);

LinkerObject *linker_object_remove_child(LinkerObject *parent, LinkerObject *child);

LinkerExeOutput linker_exe_output_init(void);

LinkerObject *linker_exe_output_alloc_obj(LinkerExeOutput *output);

LinkerObject *linker_exe_add_object(LinkerExeOutput *output);

void linker_exe_start_segment(LinkerExeOutput *output, U64 type, U64 flags);

void linker_exe_end_segment(LinkerExeOutput *output);

void linker_exe_start_section(LinkerExeOutput *output, String8 name, U64 type, U64 flags);

void linker_exe_end_section(LinkerExeOutput *output);

void linker_exe_add_section(LinkerExeOutput *output, String8 name, U64 type, U64 flags, TrieMatch match);

void linker_exe_add_alignment(LinkerExeOutput *output, U64 align);

// special segments

void linker_exe_start_relro(LinkerExeOutput *output);

static inline void linker_exe_end_relro(LinkerExeOutput *output) {
    linker_exe_end_segment(output);
}

void linker_exe_start_tls(LinkerExeOutput *output);

static inline void linker_exe_end_tls(LinkerExeOutput *output) {
    linker_exe_end_segment(output);
}

void linker_exe_add_padding(LinkerExeOutput *output, U64 padding);

void linker_exe_add_header(LinkerExeOutput *output);

void linker_exe_add_pheader(LinkerExeOutput *output);


/*
test = linker_exe_init(void)
linker_exe_start_segment(alloc, readable, executable)
linker_exe_add_section(".init")
linker_exe_start_output_section(".text")
linker_exe_add_section(".text")
linker_exe_add_section(".text.*")
linker_exe_end_output_section()
linker_exe_add_section(".fini")
linker_exe_add_alignment(PAGESIZE)
linker_exe_end_segment()
linker_exe_start_segment(alloc, read, write)
linker_exe_start_relro_segment()
linker_exe_add_section(".tdata")
linker_add_section(".tbss")
linker_exe_add_alignment(PAGESIZE)
linker_exe_end_relro_segment()
linker_add_section(".data")
linker_add_section(".bss")
linker_exe_add_alignment(PAGESIZE)
linker_exe_end_segment()
linker_exe_end_segment()
linker_exe_stack_section(".note.gnustack")


 */

void load_input_sections (LinkerExeOutput *output, ElfFile *file);

void load_input_files(LinkerExeOutput *output, ElfFile_array files);

static U32 segment_flags_from_section(U64 sh_flags);

static U8 get_loadable_segment_order(U64 flags);


LinkerSizeResult compute_positions_linker_tree(LinkerExeOutput *output, LinkerObject *obj, LinkerPositionState *state);

void linker_tree_decend_child(LinkerExeOutput *output, LinkerObject *obj, LinkerPositionState *state);

void linker_tree_process_leaf(LinkerExeOutput *output, LinkerObject *obj, LinkerPositionState *state);

// void compute_positions_linker_tree(LinkerExeOutput *output, LinkerObject *obj, LinkerPositionState *state);


void linker_position_sections(LinkerExeOutput *output);

void output_elf_exe( LinkerExeOutput *output, String8 output_filename) ;

#endif // LINKER_EXECUTABLE_GEN_H
