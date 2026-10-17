#!/usr/bin/env python3
#
# driver.py - CS612 Assignment 3 (Cache Lab) driver.
#
# Runs ./test-csim to check the correctness of your cache simulator (Part A),
# and ./test-trans on three matrix sizes to check the correctness and the
# cache performance of your transpose_submit() function (Part B).
# This is the same program the instructor uses to grade your code.
#
# Adapted from the CS:APP Cache Lab by R. E. Bryant and D. R. O'Hallaron,
# Carnegie Mellon University.
#
import argparse
import re
import subprocess
import sys

# Maximum points for each autograded component
MAXSCORE = {"csim": 40, "trans48": 10, "trans4864": 15, "trans5973": 15}

# Part B test cases: (key, label, M, N, full-credit misses, zero-credit misses)
# M = number of columns of A (= rows of B), N = number of rows of A.
TRANS_TESTS = [
    ("trans48",   "48x48",           48, 48,  700, 1300),
    ("trans4864", "M=48,N=64", 48, 64,  900, 1400),
    ("trans5973", "M=59,N=73", 59, 73, 2100, 3200),
]


def miss_score(miss, lower, upper, full_score):
    """Full score at or below `lower` misses, zero at or above `upper`,
    and a linear scale in between."""
    if miss <= lower:
        return float(full_score)
    if miss >= upper:
        return 0.0
    return round((1.0 - (miss - lower) / float(upper - lower)) * full_score, 1)


def run(cmd):
    p = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, universal_newlines=True)
    return p.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-A", action="store_true", dest="autograde",
                    help="emit an AUTORESULT_STRING line for the grading scripts")
    opts = ap.parse_args()

    print("Part A: Testing cache simulator")
    print("Running ./test-csim")
    out = run("./test-csim")
    csim_score = 0.0
    for line in out.splitlines():
        m = re.match(r"TEST_CSIM_RESULTS=([\d.]+)", line)
        if m:
            csim_score = float(m.group(1))
        else:
            print(line)

    print("Part B: Testing transpose function")
    results = {}
    for key, label, M, N, lo, hi in TRANS_TESTS:
        print("Running ./test-trans -M %d -N %d" % (M, N))
        out = run("./test-trans -M %d -N %d" % (M, N))
        m = re.search(r"TEST_TRANS_RESULTS=(\d+):(\d+)", out)
        if m:
            correct, misses = int(m.group(1)), int(m.group(2))
        else:
            print(out)
            correct, misses = 0, 2**31 - 1
        score = miss_score(misses, lo, hi, MAXSCORE[key]) * correct
        results[key] = (label, correct, misses, score)

    total = csim_score + sum(r[3] for r in results.values())
    maxtotal = sum(MAXSCORE.values())

    print("\nCache Lab summary:")
    print("%-26s%8s%10s%12s" % ("", "Points", "Max pts", "Misses"))
    print("%-26s%8.1f%10d" % ("Csim correctness", csim_score, MAXSCORE["csim"]))
    for key, label, M, N, lo, hi in TRANS_TESTS:
        _, correct, misses, score = results[key]
        shown = "invalid" if (not correct or misses == 2**31 - 1) else str(misses)
        print("%-26s%8.1f%10d%12s" % ("Trans perf " + label, score,
                                       MAXSCORE[key], shown))
    print("%26s%8.1f%10d" % ("Total points", total, maxtotal))

    if opts.autograde:
        print("\nAUTORESULT_STRING=%.1f:%.1f:%s" % (
            total, csim_score,
            ":".join("%d:%.1f" % (results[k][2], results[k][3])
                     for k, *_ in TRANS_TESTS)))


if __name__ == "__main__":
    main()
