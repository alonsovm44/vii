CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99

all: io

io: src/io.c
	$(CC) $(CFLAGS) -o io src/io.c

clean:
	rm -f io

run: io
	./io examples/fizzbuzz.io

.PHONY: all clean run
