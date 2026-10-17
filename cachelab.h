/*
 * cachelab.h - Prototypes for the CS612 Assignment 3 (Cache Lab) helpers.
 *
 * Adapted from the CS:APP Cache Lab by R. E. Bryant and D. R. O'Hallaron,
 * Carnegie Mellon University. Do not modify this file.
 */
#ifndef CACHELAB_TOOLS_H
#define CACHELAB_TOOLS_H

#define MAX_TRANS_FUNCS 100

typedef struct trans_func {
    void (*func_ptr)(int M, int N, int[N][M], int[M][N]);
    char *description;
    char correct;
    unsigned int num_hits;
    unsigned int num_misses;
    unsigned int num_evictions;
} trans_func_t;

/*
 * printSummary - The required way for your cache simulator to report its
 * final statistics. Call it exactly once, at the end of main().
 *
 *   hits                 number of accesses that hit
 *   misses               number of accesses that missed
 *   evictions            number of valid lines replaced
 *   dirty_bytes_in_cache bytes held in dirty lines when the trace ends
 *                        (number of dirty lines still in the cache * B)
 *   dirty_bytes_evicted  bytes written back to memory because a dirty line
 *                        was evicted (number of dirty evictions * B)
 */
void printSummary(long hits, long misses, long evictions,
                  long dirty_bytes_in_cache, long dirty_bytes_evicted);

/* Fill the matrices with data */
void initMatrix(int M, int N, int A[N][M], int B[M][N]);

/* The baseline transpose used to check correctness */
void correctTrans(int M, int N, int A[N][M], int B[M][N]);

/* Add the given function to the list of functions that test-trans evaluates */
void registerTransFunction(void (*trans)(int M, int N, int[N][M], int[M][N]),
                           char *desc);

#endif /* CACHELAB_TOOLS_H */
