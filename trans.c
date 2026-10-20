/*
 * trans.c - CS612 Assignment 3, Part B: matrix transpose B = A^T
 *
 * Name:        Prateek Takshak
 * Roll number: 23110256
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
 *
 * Design. transpose_submit declares no locals and hands each graded size
 * to its own helper, so the longest call chain holds at most 11 int locals.
 *
 *   48 x 48   A row = B row = 192 bytes, set(row i) = 6i mod 32, so rows 16
 *             apart collide and any 8 consecutive rows sit in 8 different
 *             sets. 8x8 tiles; each row of an A tile is read whole into
 *             8 locals before any of it is written to B, so on the diagonal
 *             tiles, where A row r and B row r share a set, a B write can
 *             only evict an A line that has already been consumed.
 *
 *   48 x 64   A row = 192 bytes, B row = 256 bytes = 8 blocks, so B rows 4
 *             apart map to the same set and an 8x8 tile of B only has 4
 *             sets to live in. Each 8x8 tile is done as four 4x4 quadrants,
 *             parking the top-right quarter of A in the unused top-right
 *             quadrant of B so that only 4 B rows are live at a time. In
 *             step 2 the A column is read before the parked B row, so the
 *             A loads cannot evict the B line that is about to be reused.
 *
 *   59 x 73   236 and 292 byte rows are not multiples of 32, so conflicts
 *             scatter and square tiles do badly. Instead, bands of 13 rows
 *             of A are walked one column at a time: the 13 A lines of a
 *             band stay cached across all 59 columns, and each column of
 *             the band is a contiguous run of 13 ints in one row of B.
 *             The band height 13 was picked by sweeping every shape.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans_48x48(int M, int N, int A[N][M], int B[M][N]);
void trans_48x64(int M, int N, int A[N][M], int B[M][N]);
void trans_blocked(int M, int N, int A[N][M], int B[M][N], int size);
void trans_blocked_rect(int M, int N, int A[N][M], int B[M][N],
                        int rows, int cols);
void trans_bands(int M, int N, int A[N][M], int B[M][N], int height);
void trans_quad_v1(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - The function graded for Part B. Do not change its
 *     description string "Transpose submission": test-trans finds the
 *     graded function by this string.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 48 && N == 48)
        trans_48x48(M, N, A, B);
    else if (M == 48 && N == 64)
        trans_48x64(M, N, A, B);
    else
        trans_bands(M, N, A, B, 13);
}

/*
 * trans_48x48 - 8x8 tiles, each A row copied through 8 locals.
 *     11 int locals.
 */
void trans_48x48(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, i;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            for (i = ii; i < ii + 8; i++) {
                a0 = A[i][jj];     a1 = A[i][jj + 1];
                a2 = A[i][jj + 2]; a3 = A[i][jj + 3];
                a4 = A[i][jj + 4]; a5 = A[i][jj + 5];
                a6 = A[i][jj + 6]; a7 = A[i][jj + 7];
                B[jj][i]     = a0; B[jj + 1][i] = a1;
                B[jj + 2][i] = a2; B[jj + 3][i] = a3;
                B[jj + 4][i] = a4; B[jj + 5][i] = a5;
                B[jj + 6][i] = a6; B[jj + 7][i] = a7;
            }
        }
    }
}

/*
 * trans_48x64 - 8x8 tiles done in three steps so that at most four rows
 *     of B (one per set) are live at once. 11 int locals.
 */
void trans_48x64(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            /*
             * Step 1: rows 0-3 of the A tile. The left halves go straight
             * to their final place; the right halves are parked, already
             * transposed, in the top-right quadrant of the same B rows.
             */
            for (k = 0; k < 4; k++) {
                a0 = A[ii + k][jj];     a1 = A[ii + k][jj + 1];
                a2 = A[ii + k][jj + 2]; a3 = A[ii + k][jj + 3];
                a4 = A[ii + k][jj + 4]; a5 = A[ii + k][jj + 5];
                a6 = A[ii + k][jj + 6]; a7 = A[ii + k][jj + 7];
                B[jj][ii + k]         = a0; B[jj + 1][ii + k]     = a1;
                B[jj + 2][ii + k]     = a2; B[jj + 3][ii + k]     = a3;
                B[jj][ii + 4 + k]     = a4; B[jj + 1][ii + 4 + k] = a5;
                B[jj + 2][ii + 4 + k] = a6; B[jj + 3][ii + 4 + k] = a7;
            }
            /*
             * Step 2: for each of B rows 0-3, first read column k of A's
             * bottom-left quadrant, then lift the parked values out of
             * B row k and put the A column in their place, then drop the
             * parked values into B row 4 + k. B row 4 + k shares a set
             * with B row k, so it replaces that line only once it is done.
             */
            for (k = 0; k < 4; k++) {
                a4 = A[ii + 4][jj + k]; a5 = A[ii + 5][jj + k];
                a6 = A[ii + 6][jj + k]; a7 = A[ii + 7][jj + k];
                a0 = B[jj + k][ii + 4]; a1 = B[jj + k][ii + 5];
                a2 = B[jj + k][ii + 6]; a3 = B[jj + k][ii + 7];
                B[jj + k][ii + 4] = a4; B[jj + k][ii + 5] = a5;
                B[jj + k][ii + 6] = a6; B[jj + k][ii + 7] = a7;
                B[jj + 4 + k][ii]     = a0; B[jj + 4 + k][ii + 1] = a1;
                B[jj + 4 + k][ii + 2] = a2; B[jj + 4 + k][ii + 3] = a3;
            }
            /*
             * Step 3: the bottom-right 4x4, one row of B at a time, so
             * each B line is written whole while it is cached.
             */
            for (k = 4; k < 8; k++) {
                a0 = A[ii + 4][jj + k]; a1 = A[ii + 5][jj + k];
                a2 = A[ii + 6][jj + k]; a3 = A[ii + 7][jj + k];
                B[jj + k][ii + 4] = a0; B[jj + k][ii + 5] = a1;
                B[jj + k][ii + 6] = a2; B[jj + k][ii + 7] = a3;
            }
        }
    }
}

/*
 * trans_quad_v1 - First version of the 48x64 scheme: step 2 reads the
 *     parked B row before the A column, and step 3 goes along rows of A.
 *     Kept for the comparison in report.md. 11 int locals.
 */
void trans_quad_v1(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            /*
             * Step 1: rows 0-3 of the A tile. The left halves go straight
             * to their final place; the right halves are parked, already
             * transposed, in the top-right quadrant of the same B rows.
             */
            for (k = 0; k < 4; k++) {
                a0 = A[ii + k][jj];     a1 = A[ii + k][jj + 1];
                a2 = A[ii + k][jj + 2]; a3 = A[ii + k][jj + 3];
                a4 = A[ii + k][jj + 4]; a5 = A[ii + k][jj + 5];
                a6 = A[ii + k][jj + 6]; a7 = A[ii + k][jj + 7];
                B[jj][ii + k]         = a0; B[jj + 1][ii + k]     = a1;
                B[jj + 2][ii + k]     = a2; B[jj + 3][ii + k]     = a3;
                B[jj][ii + 4 + k]     = a4; B[jj + 1][ii + 4 + k] = a5;
                B[jj + 2][ii + 4 + k] = a6; B[jj + 3][ii + 4 + k] = a7;
            }
            /*
             * Step 2: for each of B rows 0-3, lift the parked values out,
             * fill their slots with column k of A's bottom-left quadrant,
             * then drop the parked values into B row 4 + k, which shares
             * a set with B row k and so replaces it only once it is done.
             */
            for (k = 0; k < 4; k++) {
                a0 = B[jj + k][ii + 4]; a1 = B[jj + k][ii + 5];
                a2 = B[jj + k][ii + 6]; a3 = B[jj + k][ii + 7];
                a4 = A[ii + 4][jj + k]; a5 = A[ii + 5][jj + k];
                a6 = A[ii + 6][jj + k]; a7 = A[ii + 7][jj + k];
                B[jj + k][ii + 4] = a4; B[jj + k][ii + 5] = a5;
                B[jj + k][ii + 6] = a6; B[jj + k][ii + 7] = a7;
                B[jj + 4 + k][ii]     = a0; B[jj + 4 + k][ii + 1] = a1;
                B[jj + 4 + k][ii + 2] = a2; B[jj + 4 + k][ii + 3] = a3;
            }
            /* Step 3: the bottom-right 4x4, into B rows 4-7. */
            for (k = 4; k < 8; k++) {
                a0 = A[ii + k][jj + 4]; a1 = A[ii + k][jj + 5];
                a2 = A[ii + k][jj + 6]; a3 = A[ii + k][jj + 7];
                B[jj + 4][ii + k] = a0; B[jj + 5][ii + k] = a1;
                B[jj + 6][ii + k] = a2; B[jj + 7][ii + k] = a3;
            }
        }
    }
}

/*
 * trans_blocked - Square tiles of the given size, one element at a time,
 *     with bounds checks so any M and N work. 6 int locals.
 */
void trans_blocked(int M, int N, int A[N][M], int B[M][N], int size)
{
    trans_blocked_rect(M, N, A, B, size, size);
}

/*
 * trans_blocked_rect - Tiles of rows x cols elements of A. 6 int locals
 *     (the two size parameters are parameters, not locals).
 */
void trans_blocked_rect(int M, int N, int A[N][M], int B[M][N],
                        int rows, int cols)
{
    int ii, jj, i, j, tmp;

    for (jj = 0; jj < M; jj += cols) {
        for (ii = 0; ii < N; ii += rows) {
            for (i = ii; i < ii + rows && i < N; i++) {
                for (j = jj; j < jj + cols && j < M; j++) {
                    tmp = A[i][j];
                    B[j][i] = tmp;
                }
            }
        }
    }
}

/*
 * trans_bands - Bands of `height` rows of A, each walked one column at a
 *     time. 4 int locals.
 */
void trans_bands(int M, int N, int A[N][M], int B[M][N], int height)
{
    int ii, i, j, tmp;

    for (ii = 0; ii < N; ii += height) {
        for (j = 0; j < M; j++) {
            for (i = ii; i < ii + height && i < N; i++) {
                tmp = A[i][j];
                B[j][i] = tmp;
            }
        }
    }
}

/*
 * Experimental variants measured for the tables in report.md. They are
 * not registered by default, because test-trans runs valgrind once per
 * registered function under a single 300 second alarm; uncomment the
 * lines in registerFunctions() to reproduce the numbers.
 */
char trans_blk8_desc[] = "Blocked 8x8";
void trans_blk8(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked(M, N, A, B, 8);
}

char trans_blk16_desc[] = "Blocked 16x16";
void trans_blk16(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked(M, N, A, B, 16);
}

char trans_blk4_desc[] = "Blocked 4x4";
void trans_blk4(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked(M, N, A, B, 4);
}

char trans_rows8_desc[] = "8x8 tiles, row copied through 8 locals";
void trans_rows8(int M, int N, int A[N][M], int B[M][N])
{
    if ((M % 8) == 0 && (N % 8) == 0)
        trans_48x48(M, N, A, B);
    else
        trans_blocked(M, N, A, B, 8);
}

char trans_quad_desc[] = "Quadrants v1 (parked row read first)";
void trans_quad(int M, int N, int A[N][M], int B[M][N])
{
    if ((M % 8) == 0 && (N % 8) == 0)
        trans_quad_v1(M, N, A, B);
    else
        trans_blocked(M, N, A, B, 8);
}

char trans_quad2_desc[] = "Quadrants v2 (A column read first)";
void trans_quad2(int M, int N, int A[N][M], int B[M][N])
{
    if ((M % 8) == 0 && (N % 8) == 0)
        trans_48x64(M, N, A, B);
    else
        trans_blocked(M, N, A, B, 8);
}

char trans_strip4_desc[] = "Column strips 4 wide";
void trans_strip4(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked_rect(M, N, A, B, 16, 4);
}

char trans_band8_desc[] = "Bands of 8 rows, column by column";
void trans_band8(int M, int N, int A[N][M], int B[M][N])
{
    trans_bands(M, N, A, B, 8);
}

char trans_band13_desc[] = "Bands of 13 rows, column by column";
void trans_band13(int M, int N, int A[N][M], int B[M][N])
{
    trans_bands(M, N, A, B, 13);
}

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

    /*
     * The variants compared in report.md. Uncomment to measure them; see
     * the note above the variants for why they are off by default.
     */
    /* registerTransFunction(trans, trans_desc); */
    /* registerTransFunction(trans_blk4, trans_blk4_desc); */
    /* registerTransFunction(trans_blk8, trans_blk8_desc); */
    /* registerTransFunction(trans_blk16, trans_blk16_desc); */
    /* registerTransFunction(trans_rows8, trans_rows8_desc); */
    /* registerTransFunction(trans_quad, trans_quad_desc); */
    /* registerTransFunction(trans_quad2, trans_quad2_desc); */
    /* registerTransFunction(trans_strip4, trans_strip4_desc); */
    /* registerTransFunction(trans_band8, trans_band8_desc); */
    /* registerTransFunction(trans_band13, trans_band13_desc); */
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
