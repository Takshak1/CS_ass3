/*
 * tracegen.c - When run under Valgrind, produces a memory trace of the
 * registered transpose functions. Do not modify this file.
 *
 * The start and end of each function's accesses are marked by stores to
 * two "marker" variables, whose addresses are saved in the file .marker.
 *
 * Adapted from the CS:APP Cache Lab by R. E. Bryant and D. R. O'Hallaron,
 * Carnegie Mellon University.
 */
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cachelab.h"

/* Declared in cachelab.c */
extern trans_func_t func_list[MAX_TRANS_FUNCS];
extern int func_counter;

/* Defined in trans.c */
extern void registerFunctions(void);

/* Markers that bound the trace of each transpose function */
volatile char MARKER_START, MARKER_END;

/*
 * A and B start at 4096-byte boundaries. Because 4096 is a multiple of the
 * 1 KB simulated cache, A[0][0] and B[0][0] both map to set 0, on every
 * machine. This makes miss counts identical across machines.
 */
static int A[256][256] __attribute__((aligned(4096)));
static int B[256][256] __attribute__((aligned(4096)));
static int M;
static int N;

static int validate(int fn, int M, int N, int A[N][M], int B[M][N])
{
    static int C[256][256];
    int (*Cp)[N] = (int (*)[N])C;
    int i, j;
    correctTrans(M, N, A, Cp);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            if (B[i][j] != Cp[i][j]) {
                printf("Validation failed on function %d! Expected %d but got "
                       "%d at B[%d][%d]\n", fn, Cp[i][j], B[i][j], i, j);
                return 0;
            }
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int i, c;
    int selectedFunc = -1;

    while ((c = getopt(argc, argv, "M:N:F:")) != -1) {
        switch (c) {
        case 'M': M = atoi(optarg); break;
        case 'N': N = atoi(optarg); break;
        case 'F': selectedFunc = atoi(optarg); break;
        default:
            printf("./tracegen failed to parse its options.\n");
            exit(1);
        }
    }
    if (M <= 0 || N <= 0 || M > 256 || N > 256) {
        printf("./tracegen: M and N must be between 1 and 256\n");
        exit(1);
    }

    registerFunctions();

    initMatrix(M, N, (int (*)[M])A, (int (*)[N])B);

    FILE *marker_fp = fopen(".marker", "w");
    assert(marker_fp);
    fprintf(marker_fp, "%llx %llx",
            (unsigned long long)&MARKER_START, (unsigned long long)&MARKER_END);
    fclose(marker_fp);

    if (selectedFunc == -1) {
        for (i = 0; i < func_counter; i++) {
            MARKER_START = 33;
            (*func_list[i].func_ptr)(M, N, (int (*)[M])A, (int (*)[N])B);
            MARKER_END = 34;
            if (!validate(i, M, N, (int (*)[M])A, (int (*)[N])B))
                return i + 1;
        }
    } else {
        if (selectedFunc < 0 || selectedFunc >= func_counter) {
            printf("./tracegen: no function %d\n", selectedFunc);
            return 255;
        }
        MARKER_START = 33;
        (*func_list[selectedFunc].func_ptr)(M, N, (int (*)[M])A, (int (*)[N])B);
        MARKER_END = 34;
        if (!validate(selectedFunc, M, N, (int (*)[M])A, (int (*)[N])B))
            return selectedFunc + 1;
    }
    return 0;
}
