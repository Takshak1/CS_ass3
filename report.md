# CS 612 Assignment 3: Cache Lab Report

**Name:** Prateek Takshak
**Roll number:** 23110256
**Autograder result (`./driver.py`, fresh handout copy with only my two `.c` files):** 80 / 80. Part A 40/40, Part B misses 648 / 878 / 1928.

Terms: a *block* is 32 bytes (8 ints) in the Part B cache, the *set* is which of the 32 slots an address lands in, and two rows *collide* when they share a set, so loading one evicts the other.

## Part A: the simulator

### How the cache is stored

The cache is one `calloc`'d array of `S * E` structs (valid flag, 64-bit tag, dirty flag, `last_used` stamp). Set `i` owns entries `i*E` to `i*E + E - 1`. One allocation fits any geometry, and `calloc` starts every line invalid.

The address is split with two shifts and a mask:

- `set = (addr >> b) & (2^s - 1)`
- `tag = addr >> (s + b)`

With `s = 0` the mask is 0, so the cache is fully associative, and with `b = 0` the shift does nothing. Neither needs special code, which mattered because two graded tests use `s = 0`.

Worked example from `wb.trace` with `s = 1, b = 2`: `L 14,4`. Address `0x14` is `10100` in binary: offset `00`, set `1`, tag `10` = 2. Set 1 then holds the dirty block of `0xc` (tag 1, written by `S c,4`), so this is a miss with an eviction and 4 bytes of writeback. Both simulators print `L 14,4 miss eviction writeback`.

### LRU and dirty lines

A global counter goes up on every access and is copied into `last_used` whenever a line is hit or filled. On a miss I use an invalid line if the set has one, otherwise the line with the smallest stamp, found by scanning the set's `E` lines (at most 8 in the tests, so no list is needed).

A store hit sets `dirty = 1`, and a fill is dirty only if it was a store. Evicting a dirty line adds `B` to `dirty_bytes_evicted`, and a final sweep adds `B` per line still dirty. An `M` is two calls, a load then a store, so the store always hits.

### Bugs

Honestly, no graded test failed; the first build scored 40/40. Since the spec requires matching `csim-ref` for every geometry, I looked for hidden bugs: I diffed `-v` output against `csim-ref -v` (including `s = 0` and `b = 0`) and swept 630 configurations (`s` 0 to 8, `E` in {1, 2, 3, 4, 8}, `b` 0 to 6). Everything matched. My real problem was tooling (first barrier, last section).

## Part B: the transpose

A and B both start in set 0. `A[i][j]` is at byte `4*(M*i + j)`, so its set is `floor((M*i + j) / 8) mod 32`; B is the same with N for M. All counts are from my own `./test-trans` runs.

### 48 x 48

**Arithmetic.** A row of A or B is 48 * 4 = 192 bytes (6 blocks), so row `i` starts in set `6i mod 32`. Rows `k` apart collide when `6k` is a multiple of 32, i.e. `k = 16` (rows 0, 16, 32 collide). Any 8 consecutive rows use 8 different sets (0, 6, 12, 18, 24, 30, 4, 10), so 8x8 tiles are safe within one matrix.

For a tile at A rows `8u..8u+7` and columns `8v..8v+7`, A row `8u+r` sits in set `16u + 6r + v` and B row `8v+c` sits in set `16v + 6c + u` (mod 32). On a diagonal tile (`u = v`) these are equal exactly when `r = c`. So A row r and B row r of the same tile fight over one set.

| Strategy | Misses |
|---|---|
| Simple row-wise (handout) | 2664 |
| Blocked 4x4 | 1054 |
| Blocked 16x16 | 836 |
| Blocked 8x8, one element at a time | 778 |
| Quadrant scheme from the 48x64 case | 696 |
| **8x8 tiles, A row copied through 8 locals (final)** | **648** |

**Why the final one works, and the diagonal.** Plain 8x8 blocking alternates `A[i][j]` reads and `B[j][i]` writes, which ping-pongs on a diagonal tile. From `trace.f3`:

```
L 111000,4 miss eviction             A[0][0]: A row 0 loaded into set 0
S 151000,4 miss eviction             B[0][0]: B row 0 evicts it, same set
L 111004,4 miss eviction writeback   A[0][1]: A row 0 again, throws out dirty B row 0
```

That costs 37 misses per diagonal tile against about 18 elsewhere. The fix is to read the whole A row into 8 locals (the stack is not traced) before writing any of it, so when a B write evicts the A line, I am already done with it. By hand: 9 misses for the first row (1 A line, 8 B lines), then 2 per row (the A row evicts its partner B row, and the write brings it back), 9 + 7 * 2 = 23. A per-tile breakdown shows exactly 23 on all six diagonal tiles. The total, 648, is 72 above the floor of 576 (288 blocks each for A and B).

### 48 x 64 (A is 64 rows by 48 columns)

**Arithmetic.** A rows are 192 bytes again (collide every 16 rows). A row of B is 64 * 4 = 256 bytes, exactly a quarter of the cache, so B row `j` starts in set `8j mod 32` and **B rows 4 apart collide**. An 8-row slab of B has only 4 sets to live in.

| Strategy | Misses |
|---|---|
| Simple row-wise (handout) | 3544 |
| Blocked 8x8 | 3544 |
| Blocked 16x16 | 3544 |
| 8x8 tiles through 8 locals | 3460 |
| Blocked 4x4 | 1378 |
| Bands of 8 rows, column by column | 1015 |
| Quadrants v1 | 928 |
| **Quadrants v2 (final)** | **878** |

**Why the final one works.** Each 8x8 tile is done in 4x4 quadrants so only 4 B rows (one per set) are live:

1. A rows 0 to 3 are read whole into 8 locals. Left halves go to their final place in B rows 0 to 3; right halves are parked, transposed, in the top-right quadrant of those rows, which is overwritten later anyway.
2. For k = 0 to 3: read column k of A's bottom-left quadrant and the parked values from B row k, write the A column into B row k (finishing it), then the parked values into B row 4+k, which shares row k's set but only after row k is done.
3. The bottom-right 4x4 goes into B rows 4 to 7, still cached.

v1 read the parked B row before the A column in step 2, and those A loads sometimes evicted the B line about to be written. Swapping that order, and doing step 3 one B row at a time, gave 878 against a floor of 768.

### 59 x 73 (A is 73 rows by 59 columns)

**Arithmetic.** A rows are 59 * 4 = 236 bytes and B rows 73 * 4 = 292 bytes, neither a multiple of 32, so no two rows line up exactly. They come close: 13 * 236 = 3068 is 4 bytes short of 3 * 1024, so A row `i+13` collides with row `i` for most columns, and 7 * 292 = 2044 is 4 short of 2 * 1024, so B rows 7 apart nearly collide.

| Strategy | Misses |
|---|---|
| Simple row-wise (handout) | 4875 |
| Blocked 16x16 | 4645 |
| Blocked 8x8 | 2741 |
| Blocked 4x4 / column strips 4 wide | 2382 |
| Bands of 8 rows, column by column | 2055 |
| **Bands of 13 rows, column by column (final)** | **1928** |

**Why the final one works.** A band of 13 rows of A is walked one column at a time. 13 is the largest band with no colliding rows, so the band's 13 A lines stay cached and each is reused across several columns. Column j of the band becomes 13 consecutive ints in row j of B, filling B lines in one go. A 14-row band would contain rows 0 and 13.

## Barriers I hit, and how I got past them

**1. The test harness would not finish.** My handout was on the Windows drive under WSL (`/mnt/d`), and my first `./test-trans -M 48 -N 48` took 7.5 minutes and reported only 2 of my 7 functions. `test-trans.c` explained it: it sets `alarm(300)` and quietly gives up, and valgrind is very slow on the Windows filesystem. In a Linux-native folder one function took under a second. Because that alarm covers every registered function, my final `trans.c` registers only `transpose_submit`; the variants are still in the file, commented out of `registerFunctions()`.

**2. 48x48: textbook blocking was not enough.** Plain 8x8 blocking gave 778, above 700. Replaying `trace.f3` with `csim-ref -v` showed the misses bunched on the diagonal tiles, which led to the 8-locals fix above (648).

**3. 48x64: blocking did nothing.** 8x8 and 16x16 tiles both gave 3544, exactly the simple version. The arithmetic explained it: B rows 4 apart collide, so half of every 8-row B tile evicted the other half. The quadrant scheme fixed that (928), still just over 900.

**4. 48x64: the last 28 misses.** Trying reorderings through valgrind was slow, so I wrote a small Python model of the cache that replays the same accesses (it always matched `test-trans` to within the 4 harness accesses). With it I found the step 2 reordering in minutes: 878.

**5. 59x73: square tiles were a dead end.** The best, 4x4, gave 2382, and 16x16 gave 4645 because a 16-row tile contains rows 13 apart. So I used the model to sweep every shape from 1x1 to 32x32 in both loop orders. The winner, 13-row bands, made sense afterwards: 13 is exactly where A rows start to collide. That gave 1928.

In every case the way through was the same: stop guessing tile sizes, work out which rows collide, and pick a shape around that.
