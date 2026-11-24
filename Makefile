# Makefile for the adaC compiler
#   make            -> builds ./adac (or adac.exe with MinGW)
#   make test       -> runs the test suite (needs python3 + unicorn)
#   make clean      -> removes the build/ directory and the compiler binary
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
INC     := -Icompiler
SRC     := $(wildcard compiler/*/*.c)
GEN     := build/library_data.c
OBJ     := $(patsubst %.c,build/obj/%.o,$(SRC)) build/obj/library_data.o
EXE     := adac$(if $(findstring Windows,$(OS)),.exe,)
EMBED   := build/embed$(if $(findstring Windows,$(OS)),.exe,)

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

$(EMBED): src/tools/embed/embed.c
	@mkdir -p build
	$(CC) -O1 -o $@ $<

# the standard library (library/**/*.adac, listed in library/MODULES) is embedded into the compiler
$(GEN): $(EMBED) library/MODULES $(shell find library -name '*.adac' 2>/dev/null)
	./$(EMBED) library > $@

build/obj/%.o: %.c $(wildcard compiler/*/*.h) compiler/common.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

build/obj/library_data.o: $(GEN)
	@mkdir -p build/obj
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

test: $(EXE)
	python3 tests/harness/compiletest.py

clean:
	rm -rf build $(EXE) tests/out

.PHONY: all test clean
