/*
 * csim.c - CS612 Assignment 3, Part A: a cache simulator.
 *
 * Name:        Prateek Takshak
 * Roll number: 23110256
 *
 * Design. The cache is a single calloc'd array of cache_line_t holding
 * S * E entries, with the lines of set i living at offset i * E. One flat
 * allocation covers any S = 2^s and any associativity E, and calloc gives
 * every line valid = 0 to start.
 *
 * Each line keeps a valid flag, a 64-bit tag, a dirty flag, and last_used,
 * a stamp taken from a counter that ticks once per access. The LRU line of
 * a set is the one with the smallest stamp, found by scanning that set's E
 * lines; an invalid line is always preferred over evicting a valid one.
 *
 * An address is split with shifts alone: set = (addr >> b) & (2^s - 1) and
 * tag = addr >> (s + b). With s = 0 the mask is 0, so every address lands
 * in set 0 and the cache is fully associative; with b = 0 the shift is a
 * no-op. Neither edge case needs its own branch.
 *
 * Write-back, write-allocate. A miss fills a line for loads and stores
 * alike. Evicting a valid dirty line adds B to dirty_bytes_evicted. A
 * store, whether it hits or fills, marks its line dirty. At the end of the
 * trace the whole array is swept and every line still valid and dirty adds
 * B to dirty_bytes_in_cache. An M is replayed as a load then a store to
 * the same address, so its second access always hits.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cachelab.h"

// One line of the cache.
typedef struct {
    unsigned long tag;          // the high bits of the address
    unsigned long last_used;    // counter value when this line was last used
    int valid;                  // 1 if this line holds a block
    int dirty;                  // 1 if this block was written to
} cache_line_t;

// The cache and its size. These are set once, after reading the arguments.
static cache_line_t *cache;
static unsigned long set_mask;      // 2^s - 1 (this is 0 when s = 0)
static unsigned long block_size;    // B = 2^b bytes per block
static long num_sets;               // S = 2^s sets
static int num_lines;               // E lines per set
static int s_bits;                  // s from the command line
static int b_bits;                  // b from the command line
static int verbose;                 // 1 if -v was given

// The numbers we report at the end.
static long hits;
static long misses;
static long evictions;              // how many full lines were thrown out
static long dirty_evicted;          // bytes written back from dirty lines

// Goes up by one on every access. Used to find the least recently used line.
static unsigned long tick;

// Print how to use the program. Same text as csim-ref.
static void usage(const char *prog)
{
    printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", prog);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Verbose: print the outcome of every access.\n");
    printf("  -s <s>     Number of set index bits (S = 2^s sets; s may be 0).\n");
    printf("  -E <E>     Number of lines per set (associativity).\n");
    printf("  -b <b>     Number of block offset bits (B = 2^b bytes; b may be 0).\n");
    printf("  -t <file>  Valgrind trace file to replay.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/l14.trace\n", prog);
    printf("  linux>  %s -v -s 6 -E 8 -b 6 -t traces/mm.trace\n", prog);
}

// Do one memory access. is_store is 1 for a store and 0 for a load.
static void access_data(unsigned long addr, int is_store)
{
    unsigned long tag = addr >> (s_bits + b_bits);
    unsigned long set = (addr >> b_bits) & set_mask;
    unsigned long oldest;
    long base = (long)set * num_lines;   // where this set starts in the array
    int i;
    int victim;

    tick++;

    // Hit: is the block already in this set?
    for (i = 0; i < num_lines; i++) {
        if (cache[base + i].valid && cache[base + i].tag == tag) {
            hits++;
            cache[base + i].last_used = tick;
            if (is_store)
                cache[base + i].dirty = 1;
            if (verbose)
                printf(" hit");
            return;
        }
    }

    // Miss: we have to bring the block in (for loads and stores both).
    misses++;
    if (verbose)
        printf(" miss");

    // First try to find an empty line in the set.
    victim = -1;
    for (i = 0; i < num_lines; i++) {
        if (!cache[base + i].valid) {
            victim = i;
            break;
        }
    }

    if (victim < 0) {
        // No empty line, so throw out the one used longest ago.
        victim = 0;
        oldest = cache[base].last_used;
        for (i = 1; i < num_lines; i++) {
            if (cache[base + i].last_used < oldest) {
                oldest = cache[base + i].last_used;
                victim = i;
            }
        }
        evictions++;
        if (verbose)
            printf(" eviction");
        // If it was written to, its data has to go back to memory.
        if (cache[base + victim].dirty) {
            dirty_evicted += (long)block_size;
            if (verbose)
                printf(" writeback");
        }
    }

    // Put the new block in the chosen line.
    cache[base + victim].valid = 1;
    cache[base + victim].tag = tag;
    cache[base + victim].dirty = is_store;
    cache[base + victim].last_used = tick;
}

// Read the trace file line by line and run each L, S and M access.
// I lines (instruction fetches) are skipped. The size field is ignored.
// Returns 1 if it worked, 0 if the file could not be opened.
static int replay(const char *path)
{
    char line[256];
    char op;
    unsigned long addr;
    int size;
    FILE *fp = fopen(path, "r");

    if (!fp) {
        fprintf(stderr, "Error: cannot open trace file %s\n", path);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] != ' ')
            continue;   // I lines do not start with a space, so skip them
        if (sscanf(line, " %c %lx,%d", &op, &addr, &size) != 3)
            continue;
        if (op != 'L' && op != 'S' && op != 'M')
            continue;

        if (verbose)
            printf("%c %lx,%d", op, addr, size);

        // L and M start with a load. S is a store.
        access_data(addr, op == 'S');
        if (op == 'M')
            access_data(addr, 1);   // the store part of M

        if (verbose)
            printf("\n");
    }

    fclose(fp);
    return 1;
}

// Read the arguments, build the cache, run the trace, and print the results.
// Returns 0 if all went well, 1 if something was wrong.
int main(int argc, char *argv[])
{
    const char *trace_path = NULL;
    long dirty_in_cache = 0;
    long i;
    int opt;

    // Start at -1 so we can tell if an argument was not given.
    s_bits = -1;
    b_bits = -1;
    num_lines = -1;

    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'v':
            verbose = 1;
            break;
        case 's':
            s_bits = atoi(optarg);
            break;
        case 'E':
            num_lines = atoi(optarg);
            break;
        case 'b':
            b_bits = atoi(optarg);
            break;
        case 't':
            trace_path = optarg;
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (s_bits < 0 || b_bits < 0 || num_lines < 1 || trace_path == NULL) {
        fprintf(stderr, "%s: Missing required command line argument\n", argv[0]);
        usage(argv[0]);
        return 1;
    }
    // We shift the address by s + b, and a 64-bit number can only be
    // shifted by up to 63.
    if (s_bits + b_bits > 63) {
        fprintf(stderr, "%s: s + b must be at most 63\n", argv[0]);
        return 1;
    }

    // Work out S, B and the mask for the set bits.
    num_sets = 1L << s_bits;
    block_size = 1UL << b_bits;
    set_mask = (s_bits == 0) ? 0UL : ((1UL << s_bits) - 1UL);

    // Make the cache. calloc sets everything to 0, so all lines start empty.
    cache = calloc((size_t)num_sets * num_lines, sizeof(cache_line_t));
    if (!cache) {
        fprintf(stderr, "%s: cannot allocate a cache of %ld sets x %d lines\n",
                argv[0], num_sets, num_lines);
        return 1;
    }

    if (!replay(trace_path)) {
        free(cache);
        return 1;
    }

    // Count the bytes in lines that are still dirty at the end.
    for (i = 0; i < num_sets * num_lines; i++)
        if (cache[i].valid && cache[i].dirty)
            dirty_in_cache += (long)block_size;

    printSummary(hits, misses, evictions, dirty_in_cache, dirty_evicted);
    free(cache);
    return 0;
}

