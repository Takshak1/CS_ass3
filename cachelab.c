/*
 * cachelab.c - CS612 Assignment 3 (Cache Lab) helper functions.
 *
 * Adapted from the CS:APP Cache Lab by R. E. Bryant and D. R. O'Hallaron,
 * Carnegie Mellon University. Do not modify this file.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "cachelab.h"

trans_func_t func_list[MAX_TRANS_FUNCS];
int func_counter = 0;

/*
 * printSummary - Print the simulation statistics and save them in the file
 * .csim_results, which is where the test programs read them from. Your
 * simulator must call this function, or it cannot be graded.
 */
void printSummary(long hits, long misses, long evictions,
                  long dirty_bytes_in_cache, long dirty_bytes_evicted)
{
    printf("hits:%ld misses:%ld evictions:%ld "
           "dirty_bytes_in_cache:%ld dirty_bytes_evicted:%ld\n",
           hits, misses, evictions, dirty_bytes_in_cache, dirty_bytes_evicted);
    FILE *output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%ld %ld %ld %ld %ld\n", hits, misses, evictions,
            dirty_bytes_in_cache, dirty_bytes_evicted);
    fclose(output_fp);
}

/*
 * initMatrix - Fill A with random values and B with different random values,
 * so that a transpose that forgets to write some element of B is caught.
 */
void initMatrix(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;
    srand(time(NULL));
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            A[i][j] = rand();
            B[j][i] = rand();
        }
    }
}

/*
 * correctTrans - Baseline transpose used to check correctness.
 */
void correctTrans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerTransFunction - Add a transpose function to the list that
 * test-trans evaluates.
 */
void registerTransFunction(void (*trans)(int M, int N, int[N][M], int[M][N]),
                           char *desc)
{
    assert(func_counter < MAX_TRANS_FUNCS);
    func_list[func_counter].func_ptr = trans;
    func_list[func_counter].description = desc;
    func_list[func_counter].correct = 0;
    func_list[func_counter].num_hits = 0;
    func_list[func_counter].num_misses = 0;
    func_list[func_counter].num_evictions = 0;
    func_counter++;
}
