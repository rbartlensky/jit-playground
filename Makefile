CC ?= gcc
OUT = build
FLAGS =
CFLAGS ?= -std=c11 -pedantic -O2 -Wall -Werror -Wextra
INCL = -I ./src -I ./third-party

all: lsp

out:
	mkdir -p $(OUT)

debug: CFLAGS = -std=c11 -pedantic -g -Wall -Wextra
debug: lsp

$(OUT)/mpc.o: out third-party/mpc.c
	$(CC) -c third-party/mpc.c -o $(OUT)/mpc.o

lsp: src/*.c src/compiler/*.c $(OUT)/mpc.o
	$(CC) $(CFLAGS) -o $(OUT)/lsp $? $(INCL)

clean:
	rm -rf $(OUT)

.PHONY : all clean debug lsp out
