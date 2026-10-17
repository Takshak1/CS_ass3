/*
 * test-csim.c - Checks your cache simulator (./csim) by comparing its five
 * statistics with those of the reference simulator (./csim-ref) on the
 * test cases used for grading. Do not modify this file.
 *
 * Adapted from the CS:APP Cache Lab by R. E. Bryant and D. R. O'Hallaron,
 * Carnegie Mellon University.
 */
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_STR 1024
#define NSTATS 5   /* hits, misses, evictions, dirty_bytes_in_cache, dirty_bytes_evicted */

static void usage(char *argv[])
{
    printf("Usage: %s [-h]\n", argv[0]);
    printf("Options:\n");
    printf("  -h    Print this help message.\n");
}

static void sigalrm_handler(int signum)
{
    (void)signum;
    printf("Error: Program timed out.\n");
    printf("TEST_CSIM_RESULTS=0.0\n");
    exit(1);
}

/*
 * run_sim - Run one simulator on one test case and read the five numbers it
 * saved in .csim_results. Returns 1 on success, 0 on any problem.
 */
static int run_sim(const char *sim, int s, int E, int b, const char *trace,
                   long stats[NSTATS])
{
    char cmd[MAX_STR];
    int status;
    FILE *fp;

    sprintf(cmd, "%s -s %d -E %d -b %d -t %s > /dev/null", sim, s, E, b, trace);
    unlink(".csim_results");
    status = system(cmd);
    if (status == -1) {
        fprintf(stderr, "Error invoking system() for %s: %s\n", sim, strerror(errno));
        return 0;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Error: %s -s %d -E %d -b %d -t %s did not exit normally "
                "(status %d)\n", sim, s, E, b, trace, status);
        return 0;
    }
    fp = fopen(".csim_results", "r");
    if (!fp) {
        fprintf(stderr, "Error: no results from %s. Did you call printSummary()?\n", sim);
        return 0;
    }
    if (fscanf(fp, "%ld %ld %ld %ld %ld", &stats[0], &stats[1], &stats[2],
               &stats[3], &stats[4]) != NSTATS) {
        fclose(fp);
        fprintf(stderr, "Error: could not read results from %s\n", sim);
        return 0;
    }
    fclose(fp);
    return 1;
}

#define N 12   /* number of test cases */

static void test_csim(void)
{
    /* The test cases used for grading */
    int s[N]      = {2, 1, 1, 0, 2, 2, 0, 4, 5, 5, 6, 3};
    int E[N]      = {1, 2, 1, 2, 1, 2, 4, 2, 1, 1, 8, 4};
    int b[N]      = {1, 1, 2, 2, 3, 3, 3, 4, 5, 5, 6, 6};
    int weight[N] = {2, 2, 3, 3, 3, 3, 3, 3, 3, 5, 5, 5};
    char *trace[N] = {"traces/l14.trace", "traces/l14.trace",
                      "traces/wb.trace", "traces/wb.trace",
                      "traces/trans.trace", "traces/trans.trace",
                      "traces/trans.trace", "traces/mm.trace",
                      "traces/mm.trace", "traces/long.trace",
                      "traces/long.trace", "traces/long.trace"};

    long ref[N][NSTATS], you[N][NSTATS];
    double points[N], total = 0.0;
    int max_total = 0;
    int i, k, ok_ref, ok_you;
    char buf[MAX_STR];

    for (i = 0; i < N; i++) {
        ok_ref = run_sim("./csim-ref", s[i], E[i], b[i], trace[i], ref[i]);
        ok_you = run_sim("./csim", s[i], E[i], b[i], trace[i], you[i]);
        points[i] = 0.0;
        if (!ok_ref)
            for (k = 0; k < NSTATS; k++) ref[i][k] = -1;
        if (!ok_you)
            for (k = 0; k < NSTATS; k++) you[i][k] = -1;
        /*
         * A test earns points only if hits + misses equals the reference
         * total, i.e. your simulator counted every data access in the trace.
         * Then each of the five statistics that matches is worth 1/5.
         */
        if (ok_ref && ok_you && you[i][0] + you[i][1] == ref[i][0] + ref[i][1])
            for (k = 0; k < NSTATS; k++)
                if (ref[i][k] == you[i][k])
                    points[i] += (double)weight[i] / NSTATS;
        total += points[i];
        max_total += weight[i];
    }

    printf("Each row: hits, misses, evictions, dirty bytes left in cache, dirty bytes evicted\n\n");
    printf("%6s %10s  %-38s %-38s %s\n", "Points", "(s,E,b)",
           "Your simulator", "Reference simulator", "Trace");
    for (i = 0; i < N; i++) {
        char mine[MAX_STR], theirs[MAX_STR];
        sprintf(buf, "(%d,%d,%d)", s[i], E[i], b[i]);
        sprintf(mine, "%ld %ld %ld %ld %ld", you[i][0], you[i][1], you[i][2],
                you[i][3], you[i][4]);
        sprintf(theirs, "%ld %ld %ld %ld %ld", ref[i][0], ref[i][1], ref[i][2],
                ref[i][3], ref[i][4]);
        printf("%3.1f/%-2d %10s  %-38s %-38s %s\n", points[i], weight[i], buf,
               mine, theirs, trace[i]);
    }
    printf("%4.1f/%d\n", total, max_total);
    printf("\nTEST_CSIM_RESULTS=%.1f\n", total);
}

int main(int argc, char *argv[])
{
    int c;
    while ((c = getopt(argc, argv, "h")) != -1) {
        switch (c) {
        case 'h': usage(argv); exit(0);
        default:  usage(argv); exit(1);
        }
    }
    signal(SIGALRM, sigalrm_handler);
    alarm(120);
    test_csim();
    return 0;
}
