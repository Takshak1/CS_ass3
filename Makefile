#
# CS612 Assignment 3 (Cache Lab) - student Makefile
# Tested on x86-64 Ubuntu 24.04 (gcc 13, valgrind 3.22, python 3.12).
#
CC = gcc
CFLAGS = -g -Wall -Werror -std=gnu99

ARCH := $(shell uname -m)

all: csim-ref csim test-csim test-trans tracegen

# Pick the reference simulator binary that matches this machine.
csim-ref: bin/csim-ref-$(ARCH)
	cp bin/csim-ref-$(ARCH) csim-ref
	chmod +x csim-ref

bin/csim-ref-$(ARCH):
	@echo "No reference simulator for machine type '$(ARCH)'."
	@echo "Use x86-64 (x86_64) or 64-bit ARM (aarch64) Linux."
	@false

csim: csim.c cachelab.c cachelab.h
	$(CC) $(CFLAGS) -o csim csim.c cachelab.c -lm

test-csim: test-csim.c
	$(CC) $(CFLAGS) -o test-csim test-csim.c

test-trans: test-trans.c trans.o cachelab.c cachelab.h
	$(CC) $(CFLAGS) -o test-trans test-trans.c cachelab.c trans.o

tracegen: tracegen.c trans.o cachelab.c cachelab.h
	$(CC) $(CFLAGS) -O0 -o tracegen tracegen.c trans.o cachelab.c

# trans.c is compiled without optimization, so that every A[i][j] and B[j][i]
# in your code becomes a real memory access in the trace.
trans.o: trans.c cachelab.h
	$(CC) $(CFLAGS) -O0 -c trans.c

# make submit ROLL=<your roll number>
# Copies the files you submit into submission/, with your roll number prefixed.
submit:
	@if [ -z "$(ROLL)" ]; then echo "Usage: make submit ROLL=<your roll number>"; false; fi
	@if [ ! -f report.md ]; then echo "report.md is missing (see ASSIGNMENT.pdf, Section 8)"; false; fi
	mkdir -p submission
	cp csim.c submission/$(ROLL)_csim.c
	cp trans.c submission/$(ROLL)_trans.c
	cp report.md submission/$(ROLL)_report.md
	@echo "Upload the three files in submission/ as instructed on the course page."

clean:
	rm -f *.o csim csim-ref test-csim test-trans tracegen
	rm -f trace.all trace.f* trace.tmp .csim_results .marker
	rm -rf submission

.PHONY: all clean submit
