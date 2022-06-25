/**
 * @file memory.c
 * @brief Macros to wrap dynamic memory calls (malloc, free, realloc)
 *
 * Macros to handle dynamic memory calls (malloc, realloc and free). The macros have
 * some error detection and report.
 * @version 3
 */
#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

/**
 * Esta função deve ser utilizada para auxiliar a alocação de memória.
 * Esta função <b>não deve</b> ser chamada directamente, mas sim através
 * da macro MALLOC().
 * @param size tamanho do bloco a alocar
 * @param file nome do ficheiro
 * 	       (através da macro MALLOC)
 * @param line linha onde a função foi chamada
 * 	       (através da macro MALLOC)
 * @return O bloco de memória alocado
 * @see MALLOC
 */
void *eipa_malloc(size_t size, const int line, const char *file) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "[%d@%s][ERROR] can't malloc %zu bytes\n", line, file,
                size);
    }
    return ptr;
}

/**
 * Esta função deve ser utilizada para auxiliar a libertação de memória.
 * Esta função <b>não deve</b> ser chamada directamente, mas sim através
 * da macro FREE().
 * @param ptr endereço do ponteiro a libertar
 * @param file nome do ficheiro
 * 	       (através da macro FREE)
 * @param line linha onde a função foi chamada
 * 	       (através da macro FREE)
 * @return A função não retorna nada
 * @see FREE
 */
void eipa_free(void **ptr, const int line, const char *file) {
    (void)line;
    (void)file;
    free(*ptr);
    *ptr = NULL;
}

void *swap_bytes(void *source, void *dest, size_t num_bytes) {
    unsigned char *source_p = (unsigned char *)source;
    unsigned char *dest_p = (unsigned char *)dest;

    for (size_t i = 0; i < num_bytes; i++) {
        dest_p[i] = source_p[num_bytes - i - 1];
    }
    return dest_p;
}

/**
 * Added by: Eduardo Margarido, 2200667
 * Function added to memory.c in order to aid in the reallocation of memory (used in getting filenames from batch/dir).
 * Should be called using the macro REALLOC().
 * @param ptr: address of pointer to reallocate
 * @param size: size of block to allocate
 * @param line: line number where function was called (through macro REALLOC)
 * @param file: name of file (through macro REALLOC)
 * @return ptr: pointer to the reallocated memory block
 * @see REALLOC
 */
void *eipa_realloc(void **ptr, size_t size, const int line, const char *file) {
    ptr = realloc(*ptr, size);
    if (ptr == NULL) {
        fprintf(stderr, "[%d@%s][ERROR] can't realloc %zu bytes\n", line, file, size);
    }
    return ptr;
}