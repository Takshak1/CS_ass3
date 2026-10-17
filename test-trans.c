/*
 * test-trans.c - Checks the correctness and the cache performance of every
 * transpose function registered in trans.c, and reports the result for the
 * function described as "Transpose submission". Do not modify this file.
 *
 * Usage: ./test-trans -M <cols of A> -N <rows of A>
 *
 * Adapted from the CS:APP Cache Lab by R. E. Bryant and D. R. O'Hallaron,
 * Carnegie Mellon University.
 */
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "cachelab.h"

/* Largest matrix dimension */
#define MAXN 256

/* Simulated cache for Part B: 1 KB, direct mapped, 32-byte blocks */
#define CACHE_S 5
#define CACHE_E 1
#define CACHE_B 5

/* Description string that identifies the graded function */
#define SUBMIT_DESCRIPTION "Transpose submission"

extern void registerFunctions(void);
extern trans_func_t func_list[MAX_TRANS_FUNCS];
extern int func_counter;

static int M = 0;
static int N = 0;

struct results {
    int funcid;
    int correct;
    int misses;
};
static struct results results = {-1, 0, INT_MAX};

static void eval_perf(unsigned int s, unsigned int E, unsigned int b)
{
    int i, flag, status;
    unsigned int len;
    long hits, misses, evictions, dirty_in, dirty_ev;
    unsigned long long marker_start, marker_end, addr;
    char buf[1000], cmd[512], filename[128];
    FILE *full_trace_fp, *part_trace_fp, *marker_fp, *in_fp;

    registerFunctions();

    for (i = 0; i < func_counter; i++) {
        if (strcmp(func_list[i].description, SUBMIT_DESCRIPTION) == 0)
            results.funcid = i;

        printf("\nFunction %d (%d total): %s\n", i, func_counter,
               func_list[i].description);
        printf("Step 1: Validating and generating memory traces\n");
        fflush(stdout);

        sprintf(cmd, "valgrind --tool=lackey --trace-mem=yes --log-fd=1 -v "
                     "./tracegen -M %d -N %d -F %d > trace.tmp", M, N, i);
        status = system(cmd);
        flag = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (flag != 0) {
            printf("Validation error in function %d! Run ./tracegen -M %d -N %d "
                   "-F %d for details.\nSkipping performance evaluation for "
                   "this function.\n", i, M, N, i);
            continue;
        }

        marker_fp = fopen(".marker", "r");
        assert(marker_fp);
        if (fscanf(marker_fp, "%llx %llx", &marker_start, &marker_end) != 2) {
            printf("Error: could not read .marker\n");
            exit(1);
        }
        fclose(marker_fp);

        func_list[i].correct = 1;
        if (results.funcid == i)
            results.correct = 1;

        full_trace_fp = fopen("trace.tmp", "r");
        assert(full_trace_fp);
        sprintf(filename, "trace.f%d", i);
        part_trace_fp = fopen(filename, "w");
        assert(part_trace_fp);

        /* Keep the data accesses between the two markers. */
        flag = 0;
        while (fgets(buf, sizeof(buf), full_trace_fp) != NULL) {
            if (buf[0] == ' ' && buf[2] == ' ' &&
                (buf[1] == 'S' || buf[1] == 'M' || buf[1] == 'L')) {
                if (sscanf(buf + 3, "%llx,%u", &addr, &len) != 2)
                    continue;
                if (addr == marker_start)
                    flag = 1;
                /*
                 * Valgrind adds many stack accesses that have nothing to do
                 * with your code. All stack accesses are therefore dropped,
                 * by keeping only addresses in the low 4 GB, where the
                 * global matrices A and B live. This is why Part B limits
                 * the number and type of your local variables.
                 */
                if (flag && addr < 0xffffffffULL)
                    fputs(buf, part_trace_fp);
                if (addr == marker_end) {
                    flag = 0;
                    break;
                }
            }
        }
        fclose(part_trace_fp);
        fclose(full_trace_fp);

        printf("Step 2: Evaluating performance (s=%u, E=%u, b=%u)\n", s, E, b);
        sprintf(cmd, "./csim-ref -s %u -E %u -b %u -t trace.f%d > /dev/null",
                s, E, b, i);
        unlink(".csim_results");
        if (system(cmd) != 0) {
            printf("Error: ./csim-ref failed\n");
            exit(1);
        }
        in_fp = fopen(".csim_results", "r");
        assert(in_fp);
        if (fscanf(in_fp, "%ld %ld %ld %ld %ld", &hits, &misses, &evictions,
                   &dirty_in, &dirty_ev) != 5) {
            printf("Error: could not read .csim_results\n");
            exit(1);
        }
        fclose(in_fp);
        func_list[i].num_hits = hits;
        func_list[i].num_misses = misses;
        func_list[i].num_evictions = evictions;
        printf("func %d (%s): hits:%ld, misses:%ld, evictions:%ld\n",
               i, func_list[i].description, hits, misses, evictions);

        if (results.funcid == i)
            results.misses = misses;
    }
}

static void usage(char *argv[])
{
    printf("Usage: %s [-h] -M <cols> -N <rows>\n", argv[0]);
    printf("Options:\n");
    printf("  -h          Print this help message.\n");
    printf("  -M <cols>   Number of columns of A (= rows of B), max %d\n", MAXN);
    printf("  -N <rows>   Number of rows of A (= columns of B), max %d\n", MAXN);
    printf("Example: %s -M 48 -N 48\n", argv[0]);
}

static void sigsegv_handler(int signum)
{
    (void)signum;
    printf("Error: Segmentation Fault.\n");
    printf("TEST_TRANS_RESULTS=0:0\n");
    fflush(stdout);
    exit(1);
}

static void sigalrm_handler(int signum)
{
    (void)signum;
    printf("Error: Program timed out.\n");
    printf("TEST_TRANS_RESULTS=0:0\n");
    fflush(stdout);
    exit(1);
}

int main(int argc, char *argv[])
{
    int c;

    while ((c = getopt(argc, argv, "M:N:h")) != -1) {
        switch (c) {
        case 'M': M = atoi(optarg); break;
        case 'N': N = atoi(optarg); break;
        case 'h': usage(argv); exit(0);
        default:  usage(argv); exit(1);
        }
    }

    if (M <= 0 || N <= 0) {
        printf("Error: Missing or invalid required argument\n");
        usage(argv);
        exit(1);
    }
    if (M > MAXN || N > MAXN) {
        printf("Error: M or N exceeds %d\n", MAXN);
        usage(argv);
        exit(1);
    }
    if (system("valgrind --version > /dev/null 2>&1") != 0) {
        printf("Error: valgrind is not installed. "
               "On Ubuntu: sudo apt install valgrind\n");
        exit(1);
    }

    signal(SIGSEGV, sigsegv_handler);
    signal(SIGALRM, sigalrm_handler);
    alarm(300);

    eval_perf(CACHE_S, CACHE_E, CACHE_B);

    if (results.funcid == -1) {
        printf("\nError: We could not find your transpose_submit() function\n");
        printf("Error: Please ensure that its description string is exactly \"%s\"\n",
               SUBMIT_DESCRIPTION);
        printf("\nTEST_TRANS_RESULTS=0:0\n");
    } else {
        printf("\nSummary for official submission (func %d): correctness=%d misses=%d\n",
               results.funcid, results.correct, results.misses);
        printf("\nTEST_TRANS_RESULTS=%d:%d\n", results.correct, results.misses);
    }
    return 0;
}
