CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99

SRCS = src/main.c src/ui.c src/value.c src/lexer.c src/parser.c src/interp.c src/codegen.c
OBJS = $(SRCS:.c=.o)

all: io

io: $(OBJS)
	$(CC) $(CFLAGS) -o io $(OBJS)

src/%.o: src/%.c src/io.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f io src/*.o

run: io
	./io examples/fizzbuzz.io

.PHONY: all clean run
