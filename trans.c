/*
 * trans.c - CS612 Assignment 3, Part B: matrix transpose B = A^T
 *
 * Name:        <Prateek Takshak>
 * Roll number: <23110256>
 *
 * Each transpose function must have a prototype of the form:
 *     void trans(int M, int N, int A[N][M], int B[M][N]);
 * A has N rows and M columns; B has M rows and N columns.
 *
 * A transpose function is evaluated by counting its misses on a 1 KB
 * direct-mapped cache with 32-byte blocks (s = 5, E = 1, b = 5).
 *
 * Rules (ASSIGNMENT.pdf, Section 5.3): at most 12 local variables of type
 * int per transpose function (helpers included); no other local types (no
 * long, no pointers); no arrays and no global or static variables except
 * the char ..._desc[] description strings; no malloc; no packing of several
 * values into one variable; no recursion; and A must not be modified.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - The function graded for Part B. Do not change its
 *     description string "Transpose submission": test-trans finds the
 *     graded function by this string.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
}

/*
 * You may define and register more transpose functions below, to compare
 * strategies. Here is a simple one to get you started.
 */

/*
 * trans - A simple baseline transpose, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
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
 * registerFunctions - Registers your transpose functions with test-trans.
 *     At run time, test-trans evaluates every registered function and
 *     prints its miss count. This lets you compare several strategies in
 *     one run.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);
}

/*
 * is_transpose - Returns 1 if B is the transpose of A, else 0. You can call
 *     it while debugging. It is not counted by test-trans unless you call it
 *     inside a registered function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}
