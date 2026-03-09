#ifndef LINKER_ERROR_H
#define LINKER_ERROR_H
typedef enum LinkerError {
    LINKER_ERROR_NONE, // no error
    LINKER_ERROR_GENERIC, // generic error
    LINKER_ERROR_SYM_DEFINED, // symbol exists
    LINKER_ERROR_SYM_DNE, //symbol DNE
    LINKER_ERROR_WEAK_SYM_DEFINED, // WEAK SYMBOL ALREADY DEFINED

    LINKER_ERROR_COUNT,
} LinkerError;
#endif // LINKER_ERROR_H
