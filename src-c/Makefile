CC=gcc
#CC=g++

DEBUG=-g
#DEBUG=-g -O0
#DEBUG=-g -O0 -fsanitize=address -fsanitize=undefined
#DEBUG= -fanalyzer
#DEBUG=-g -O0 --coverage
#DEBUG=-g -pg 
#
# Enable static analysis. Can be used with gcc-10 or newer compilers.
#DEBUG= -fanalyzer
#
# Enable this flag for code coverage. For example: gcov hash-lib.c
#DEBUG=-O0 --coverage
#
# Enable run-time address sanitizer.
#DEBUG= -fsanitize=address
#
# Enable run-time sanitizer for unpredictable behavior.
#DEBUG= -fsanitize=undefined
#
# Enable performance profiling (gprof).
#DEBUG=  -g -pg 
#
# Enable debugging with gdb without optimization.
#DEBUG= -g -O0

MARCH=-pthread
uname_s := $(shell uname -s)
ifeq ($(uname_s),Linux)
MARCH += -march=native
endif


all:
	$(CC) -O3 -Wall -c -fPIC avl-lib.c $(DEBUG) $(MARCH) -o avl-lib.o
	$(CC) -O3 -Wall -c -fPIC trie-lib.c $(DEBUG) $(MARCH) -o trie-lib.o
	$(CC) -O3 -Wall -c -fPIC hash-lib.c $(DEBUG) $(MARCH)  -o hash-lib.o
	$(CC) -O3 -Wall -c -fPIC lpmht-util.c $(DEBUG)  $(MARCH) -o lpmht-util.o
	$(CC) -O3 -Wall -c -fPIC lpmht-api.c $(DEBUG)  $(MARCH) -o lpmht-api.o
	ar r lpmht.a trie-lib.o hash-lib.o lpmht-util.o lpmht-api.o avl-lib.o
	$(CC) -O3 -Wall  $(DEBUG) $(MARCH)  -o test-perf test-perf.c lpmht.a 
	$(CC) -O3 -Wall  $(DEBUG) $(MARCH)  -o test-lpmht test-lpmht.c lpmht.a

clean:
	rm -f test-perf test-lpmht
	rm -f *.o *.a *.gcov *.gcda *.gcno *.out
	rm -rf *.dSYM
