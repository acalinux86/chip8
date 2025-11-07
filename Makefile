CC=gcc
CFLAGS=-Wall -Wextra -ggdb -std=c99
LIBS=-lm -lSDL2

.PHONY: build clean all

all: build/chip8

build:
	mkdir -p build

build/chip8: src/chip8.c | build
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -rf build
