typedef enum LinkerError {
    LINKER_ERROR_NONE, // no error
    LINKER_ERROR_GENERIC, // generic error
    LINKER_ERROR_SYM_DEFINED, // symbol exists
    LINKER_ERROR_SYM_DNE, //symbol DNE


    LINKER_ERROR_COUNT,
} LinkerError;
