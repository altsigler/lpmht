#CC=gcc
CC=g++
#CC=g++-14

#DEBUG=-g
#DEBUG=-g -O0
#DEBUG=-g -O0 -fsanitize=address -fsanitize=undefined
#DEBUG=-g -O0 -fsanitize=thread
#DEBUG= -fanalyzer
#DEBUG=-g -O0 --coverage
#DEBUG=-g -pg 
#
# Enable static analysis. Can be used with gcc-10 or newer compilers.
# According to the gcc documentation this option is only for C code.
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

#MARCH=-pthread
uname_s := $(shell uname -s)
ifeq ($(uname_s),Linux)
MARCH += -march=native
else
MARCH += -fexperimental-library
endif

CFLAGS= -Wall -Wextra -Wshadow -Werror
#CFLAGS+= -std=c++23
CFLAGS+= -std=c++20
#CFLAGS += -fmodules-ts

all:
	$(CC) -O3 $(CFLAGS) -c -fPIC avl-lib.cpp $(DEBUG) $(MARCH) -o avl-lib.o
	$(CC) -O3 $(CFLAGS) -c -fPIC trie-lib.cpp $(DEBUG) $(MARCH) -o trie-lib.o
	$(CC) -O3 $(CFLAGS) -c -fPIC hash-lib.cpp $(DEBUG) $(MARCH)  -o hash-lib.o
	$(CC) -O3 $(CFLAGS) -c -fPIC lpmht-api.cpp $(DEBUG)  $(MARCH) -o lpmht-api.o
	ar r lpmht.a trie-lib.o hash-lib.o lpmht-api.o avl-lib.o
	$(CC) -O3 $(CFLAGS)  $(DEBUG) $(MARCH)  -o test-perf test-perf.cpp lpmht.a 
	$(CC) -O3 $(CFLAGS)  $(DEBUG) $(MARCH)  -o test-lpmht test-lpmht.cpp lpmht.a

clean:
	rm -f test-perf test-lpmht
	rm -f *.o *.a *.gcov *.gcda *.gcno *.out
	rm -rf *.dSYM
	rm -rf gcm.cache
