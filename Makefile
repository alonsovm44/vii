CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99

SRCS = src/main.c src/ui.c src/value.c src/lexer.c src/parser.c src/interp.c src/codegen.c
OBJS = $(SRCS:.c=.o)

all: vii

vii: $(OBJS)
	$(CC) $(CFLAGS) -o vii $(OBJS)

src/%.o: src/%.c src/vii.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f vii src/*.o


	./vii examples/fizzbuzz.vii

.PHONY: all clean run
