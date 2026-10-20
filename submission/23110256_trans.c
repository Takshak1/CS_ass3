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

// My plan: transpose_submit has no locals. It just calls a different
// helper for each size, and no helper has more than 11 int locals.
//
//   48 x 48: a row is 192 bytes, so row i starts in set 6i mod 32. Rows 16
//            apart land in the same set, but any 8 rows next to each other
//            land in 8 different sets. So I use 8x8 tiles. I copy each row
//            of the tile into 8 local variables first, then write them to
//            B. On the diagonal, A row r and B row r share a set, so a B
//            write can throw out the A line, but by then I am done with it.
//
//   48 x 64: a row of B is 256 bytes, a quarter of the cache, so B rows 4
//            apart land in the same set. 8 rows of B only get 4 sets. So I
//            do each 8x8 tile in four 4x4 pieces and only ever write to 4
//            B rows at once. I park part of the data in a corner of B that
//            gets overwritten later anyway.
//
//   59 x 73: rows are 236 and 292 bytes, which do not line up with the
//            32-byte lines, so square tiles do badly. Instead I take 13
//            rows of A at a time and go down them one column at a time.
//            A rows 13 apart land in the same set, so 13 is the biggest
//            group that fits without fighting. I found 13 by trying every
//            shape.

#include <stdio.h>
#include "cachelab.h"

// Declare the functions first, so they can be used before they are written.
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
    // Use the best method for each size.
    if (M == 48 && N == 48)
        trans_48x48(M, N, A, B);
    else if (M == 48 && N == 64)
        trans_48x64(M, N, A, B);
    else
        trans_bands(M, N, A, B, 13);
}

// trans_48x48 - 8x8 tiles. Each row of the tile is copied into 8 local
// variables before it is written to B. Uses 11 int locals.
void trans_48x48(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, i;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            for (i = ii; i < ii + 8; i++) {
                // First read all 8 values of this row (one cache line).
                a0 = A[i][jj];     a1 = A[i][jj + 1];
                a2 = A[i][jj + 2]; a3 = A[i][jj + 3];
                a4 = A[i][jj + 4]; a5 = A[i][jj + 5];
                a6 = A[i][jj + 6]; a7 = A[i][jj + 7];
                // Then write them down column i of B. If a write throws
                // out the A line, that is fine, we already have the values.
                B[jj][i]     = a0; B[jj + 1][i] = a1;
                B[jj + 2][i] = a2; B[jj + 3][i] = a3;
                B[jj + 4][i] = a4; B[jj + 5][i] = a5;
                B[jj + 6][i] = a6; B[jj + 7][i] = a7;
            }
        }
    }
}

// trans_48x64 - 8x8 tiles done in three steps, so that we never write to
// more than 4 rows of B at once (one per set). Uses 11 int locals.
void trans_48x64(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            // Step 1: the top 4 rows of the A tile.
            // The left 4 values of each row go to their real place in B.
            // The right 4 values are parked (already flipped) in the
            // top-right corner of the same B rows, which we fix later.
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
            // Step 2: for each of B rows 0 to 3:
            //   - read column k of the bottom-left part of A,
            //   - take the parked values out of B row k,
            //   - put the A column in their place (B row k is now done),
            //   - move the parked values down to B row 4 + k.
            // B row 4 + k uses the same set as B row k, but row k is done.
            // Reading A first means the A reads cannot throw out the B line
            // we are about to use.
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
            // Step 3: the bottom-right 4x4 part. We fill one B row at a
            // time, so each B line is written fully while it is in cache.
            for (k = 4; k < 8; k++) {
                a0 = A[ii + 4][jj + k]; a1 = A[ii + 5][jj + k];
                a2 = A[ii + 6][jj + k]; a3 = A[ii + 7][jj + k];
                B[jj + k][ii + 4] = a0; B[jj + k][ii + 5] = a1;
                B[jj + k][ii + 6] = a2; B[jj + k][ii + 7] = a3;
            }
        }
    }
}

// trans_quad_v1 - My first try at 48x64. Same idea as trans_48x64, but in
// step 2 it reads the parked B row before the A column, and step 3 goes
// along rows of A. Kept to compare with in report.md. Uses 11 int locals.
void trans_quad_v1(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            // Step 1: same as in trans_48x64.
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
            // Step 2: take the parked values out of B row k first, then
            // read the A column. The A reads can sometimes throw out the
            // B line we still need, which is why this version is slower.
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
            // Step 3: the bottom-right 4x4 part, one A row at a time.
            for (k = 4; k < 8; k++) {
                a0 = A[ii + k][jj + 4]; a1 = A[ii + k][jj + 5];
                a2 = A[ii + k][jj + 6]; a3 = A[ii + k][jj + 7];
                B[jj + 4][ii + k] = a0; B[jj + 5][ii + k] = a1;
                B[jj + 6][ii + k] = a2; B[jj + 7][ii + k] = a3;
            }
        }
    }
}

// trans_blocked - Square tiles of size x size. It just calls
// trans_blocked_rect with the same number of rows and columns.
// No locals of its own.
void trans_blocked(int M, int N, int A[N][M], int B[M][N], int size)
{
    trans_blocked_rect(M, N, A, B, size, size);
}

// trans_blocked_rect - Tiles of rows x cols, copying one value at a time.
// The checks against N and M make it work even when the size is not a
// multiple of the tile. Uses 5 int locals (rows and cols are parameters).
void trans_blocked_rect(int M, int N, int A[N][M], int B[M][N],
                        int rows, int cols)
{
    int ii, jj, i, j, tmp;

    // (ii, jj) is the top-left corner of the tile we are copying.
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

// trans_bands - Take `height` rows of A at a time and copy them one column
// at a time. With height 13 (used for 59x73) the 13 A lines all fit in the
// cache together, so they get reused for the next columns. Each column of
// the band is 13 ints in a row in B. Uses 4 int locals (height is a
// parameter).
void trans_bands(int M, int N, int A[N][M], int B[M][N], int height)
{
    int ii, i, j, tmp;

    // ii is the first row of the band, j is the column we are copying.
    for (ii = 0; ii < N; ii += height) {
        for (j = 0; j < M; j++) {
            for (i = ii; i < ii + height && i < N; i++) {
                tmp = A[i][j];
                B[j][i] = tmp;
            }
        }
    }
}

// The other versions I tried, for the tables in report.md.
// They are not registered, because test-trans gives up after 300 seconds
// for all registered functions together. To measure them, remove the //
// in front of their lines in registerFunctions().
//
// The numbers after each one are its misses for 48x48 / 48x64 / 59x73.

// trans_blk8 - Square 8x8 tiles. 778 / 3544 / 2741.
char trans_blk8_desc[] = "Blocked 8x8";
void trans_blk8(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked(M, N, A, B, 8);
}

// trans_blk16 - Square 16x16 tiles. 836 / 3544 / 4645.
// Very bad for 59x73, because a 16-row tile has A rows 13 apart, and those
// land in the same set.
char trans_blk16_desc[] = "Blocked 16x16";
void trans_blk16(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked(M, N, A, B, 16);
}

// trans_blk4 - Square 4x4 tiles. 1054 / 1378 / 2382.
char trans_blk4_desc[] = "Blocked 4x4";
void trans_blk4(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked(M, N, A, B, 4);
}

// trans_rows8 - The 48x48 method used on other sizes too.
// 648 / 3460 / 2741. Bad for 48x64, where the 8 B rows of a tile only
// get 4 sets.
char trans_rows8_desc[] = "8x8 tiles, row copied through 8 locals";
void trans_rows8(int M, int N, int A[N][M], int B[M][N])
{
    if ((M % 8) == 0 && (N % 8) == 0)
        trans_48x48(M, N, A, B);
    else
        trans_blocked(M, N, A, B, 8);
}

// trans_quad - My first 48x64 method (trans_quad_v1). 722 / 928 / 2741.
char trans_quad_desc[] = "Quadrants v1 (parked row read first)";
void trans_quad(int M, int N, int A[N][M], int B[M][N])
{
    if ((M % 8) == 0 && (N % 8) == 0)
        trans_quad_v1(M, N, A, B);
    else
        trans_blocked(M, N, A, B, 8);
}

// trans_quad2 - My final 48x64 method (A column read first).
// 696 / 878 / 2741.
char trans_quad2_desc[] = "Quadrants v2 (A column read first)";
void trans_quad2(int M, int N, int A[N][M], int B[M][N])
{
    if ((M % 8) == 0 && (N % 8) == 0)
        trans_48x64(M, N, A, B);
    else
        trans_blocked(M, N, A, B, 8);
}

// trans_strip4 - Tiles 16 rows tall and 4 columns wide. My first 59x73
// try. 1054 / 1378 / 2382.
char trans_strip4_desc[] = "Column strips 4 wide";
void trans_strip4(int M, int N, int A[N][M], int B[M][N])
{
    trans_blocked_rect(M, N, A, B, 16, 4);
}

// trans_band8 - Groups of 8 rows. 778 / 1015 / 2055.
char trans_band8_desc[] = "Bands of 8 rows, column by column";
void trans_band8(int M, int N, int A[N][M], int B[M][N])
{
    trans_bands(M, N, A, B, 8);
}

// trans_band13 - Groups of 13 rows, my final 59x73 method.
// 987 / 1319 / 1928.
char trans_band13_desc[] = "Bands of 13 rows, column by column";
void trans_band13(int M, int N, int A[N][M], int B[M][N])
{
    trans_bands(M, N, A, B, 13);
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
    // The other versions from report.md. Remove the // to test them.
    // registerTransFunction(trans, trans_desc);
    // registerTransFunction(trans_blk4, trans_blk4_desc);
    // registerTransFunction(trans_blk8, trans_blk8_desc);
    // registerTransFunction(trans_blk16, trans_blk16_desc);
    // registerTransFunction(trans_rows8, trans_rows8_desc);
    // registerTransFunction(trans_quad, trans_quad_desc);
    // registerTransFunction(trans_quad2, trans_quad2_desc);
    // registerTransFunction(trans_strip4, trans_strip4_desc);
    // registerTransFunction(trans_band8, trans_band8_desc);
    // registerTransFunction(trans_band13, trans_band13_desc);
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
